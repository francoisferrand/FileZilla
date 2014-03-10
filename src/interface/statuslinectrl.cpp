#include <filezilla.h>
#include <wx/gauge.h>
#include "queue.h"
#include "statuslinectrl.h"
#include "Options.h"
#include "sizeformatting.h"

BEGIN_EVENT_TABLE(CStatusLineCtrl, wxWindow)
//EVT_PAINT(CStatusLineCtrl::OnPaint)
EVT_TIMER(wxID_ANY, CStatusLineCtrl::OnTimer)
//EVT_ERASE_BACKGROUND(CStatusLineCtrl::OnEraseBackground)
END_EVENT_TABLE()

#define PROGRESSBAR_WIDTH 102

CStatusLineCtrl::CStatusLineCtrl(CQueueView* pParent, const t_EngineData* const pEngineData, const wxRect& initialPosition)
	: m_pEngineData(pEngineData)
{
	wxASSERT(pEngineData);

#ifdef __WXMSW__
	Create(pParent, wxID_ANY, initialPosition.GetPosition(), initialPosition.GetSize());
#else
	Create(pParent->GetMainWindow(), wxID_ANY, initialPosition.GetPosition(), initialPosition.GetSize());
#endif

	SetOwnFont(pParent->GetFont());
	SetForegroundColour(pParent->GetForegroundColour());
	SetBackgroundStyle(wxBG_STYLE_CUSTOM);
	SetBackgroundColour(pParent->GetBackgroundColour());

	m_transferStatusTimer.SetOwner(this);

	m_pParent = pParent;
	m_pStatus = 0;
	m_lastOffset = -1;

	m_gcLastTimeStamp = wxDateTime::Now();
	m_gcLastOffset = -1;
	m_gcLastSpeed = -1;

	SetTransferStatus(0);

	m_elapsedLabel = new wxStaticText(this, wxID_ANY, wxTimeSpan(100, 0, 0).Format(_("%H:%M:%S elapsed")),
									  wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	m_remainingLabel = new wxStaticText(this, wxID_ANY, wxTimeSpan(100, 0, 0).Format(_("%H:%M:%S left")),
										wxDefaultPosition, wxDefaultSize, wxALIGN_RIGHT|wxST_NO_AUTORESIZE);
	m_progressBar = new wxGauge(this, wxID_ANY, 1);
	m_progressBar->SetMinSize(wxSize(PROGRESSBAR_WIDTH, 0));
	m_progressLabel = new wxStaticText(this, wxID_ANY, _T(">100.0%"));
	m_bytes_and_rateLabel = new wxStaticText(this, wxID_ANY, _T(""));

	wxBoxSizer * sizer = new wxBoxSizer(wxHORIZONTAL);
	sizer->AddSpacer(50);
	sizer->Add(m_elapsedLabel, 0, wxALIGN_CENTER_VERTICAL|wxFIXED_MINSIZE|wxRIGHT, 10);
	sizer->Add(m_remainingLabel, 0, wxALIGN_CENTER_VERTICAL|wxFIXED_MINSIZE|wxRIGHT, 10);
	sizer->Add(m_progressBar, 0, wxEXPAND|wxALL, 2);
	sizer->Add(m_progressLabel, 0, wxALIGN_CENTER_VERTICAL|wxFIXED_MINSIZE|wxRIGHT, 10);
	sizer->Add(m_bytes_and_rateLabel, 1, wxALIGN_CENTER_VERTICAL);
	SetSizer(sizer);
	Layout();

	m_elapsedLabel->SetLabel(_T(""));
	m_remainingLabel->SetLabel(_T(""));
	m_progressLabel->SetLabel(_T(""));
}

CStatusLineCtrl::~CStatusLineCtrl()
{
	if (m_pStatus && m_pStatus->totalSize >= 0)
		m_pEngineData->pItem->SetSize(m_pStatus->totalSize);

	if (m_transferStatusTimer.IsRunning())
		m_transferStatusTimer.Stop();
	delete m_pStatus;
}

void CStatusLineCtrl::SetTransferStatus(const CTransferStatus* pStatus)
{
	if (!pStatus)
	{
		if (m_pStatus && m_pStatus->totalSize >= 0)
			m_pParent->UpdateItemSize(m_pEngineData->pItem, m_pStatus->totalSize);
		delete m_pStatus;
		m_pStatus = 0;

		switch (m_pEngineData->state)
		{
		case t_EngineData::disconnect:
			m_statusText = _("Disconnecting from previous server");
			break;
		case t_EngineData::cancel:
			m_statusText = _("Waiting for transfer to be cancelled");
			break;
		case t_EngineData::connect:
			m_statusText = wxString::Format(_("Connecting to %s"), m_pEngineData->lastServer.FormatServer().c_str());
			break;
		default:
			m_statusText = _("Transferring");
			break;
		}

		if (m_transferStatusTimer.IsRunning())
			m_transferStatusTimer.Stop();

		m_past_data_index = -1;
	}
	else
	{
		if (!m_pStatus)
			m_pStatus = new CTransferStatus(*pStatus);
		else
			*m_pStatus = *pStatus;

		m_lastOffset = pStatus->currentOffset;

		if (!m_transferStatusTimer.IsRunning())
			m_transferStatusTimer.Start(100);
	}
	Refresh(false);
}

void CStatusLineCtrl::OnTimer(wxTimerEvent& event)
{
	bool changed;
	CTransferStatus status;

	if (!m_pEngineData || !m_pEngineData->pEngine)
	{
		m_transferStatusTimer.Stop();
		return;
	}

	if (!m_pEngineData->pEngine->GetTransferStatus(status, changed))
		SetTransferStatus(0);
	else if (changed)
	{
		if (status.madeProgress && !status.list &&
			m_pEngineData->pItem->GetType() == QueueItemType_File)
		{
			CFileItem* pItem = (CFileItem*)m_pEngineData->pItem;
			pItem->set_made_progress(true);
		}
		SetTransferStatus(&status);
	}
	else
		m_transferStatusTimer.Stop();

	//Update fields
	int elapsed_seconds = 0;
    if (m_pStatus && m_pStatus->started.IsValid())
		elapsed_seconds = wxDateTime::Now().Subtract(m_pStatus->started).GetSeconds().GetLo(); // Assume GetHi is always 0

	wxFileOffset rate;
	if (COptions::Get()->GetOptionVal(OPTION_SPEED_DISPLAY))
		rate = GetCurrentSpeed();
	else
		rate = GetSpeed(elapsed_seconds);

	CSizeFormat::_format format = static_cast<CSizeFormat::_format>(COptions::Get()->GetOptionVal(OPTION_SIZE_FORMAT));
    const wxString bytestr = CSizeFormat::Format(m_pStatus ? m_pStatus->currentOffset : 0, true, format,
												 COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0,
												 COptions::Get()->GetOptionVal(OPTION_SIZE_DECIMALPLACES));
	if (elapsed_seconds && rate > -1)
	{
		if (format == CSizeFormat::bytes)
			format = CSizeFormat::iec;
		const wxString ratestr = CSizeFormat::Format(rate, true,
													 format,
													 COptions::Get()->GetOptionVal(OPTION_SIZE_USETHOUSANDSEP) != 0,
													 COptions::Get()->GetOptionVal(OPTION_SIZE_DECIMALPLACES));
		m_bytes_and_rateLabel->SetLabel(wxString::Format(_("%s (%s/s)"), bytestr.c_str(), ratestr.c_str()));
	}
	else
		m_bytes_and_rateLabel->SetLabel(wxString::Format(_("%s (? B/s)"), bytestr.c_str()));

	wxFileOffset left = -1;
    if (m_pStatus && m_pStatus->totalSize > 0 && elapsed_seconds && rate > 0)
	{
		wxFileOffset r = m_pStatus->totalSize - m_pStatus->currentOffset;
		left = r / rate + 1;
		if (r)
			++left;

		if (left < 0)
			left = 0;
	}

	m_elapsedLabel->SetLabel(wxTimeSpan(0, 0, elapsed_seconds).Format(_("%H:%M:%S elapsed")));
	m_remainingLabel->SetLabel(left != -1 ? wxTimeSpan(0, 0, left).Format(_("%H:%M:%S left")).c_str()
										  : _("--:--:-- left"));

    if (m_pStatus && m_pStatus->totalSize > 0) {
		m_progressBar->SetRange(m_pStatus->totalSize);
		m_progressBar->SetValue(m_pStatus->currentOffset);

		int permill;
		wxString prefix;
		if (m_pStatus->currentOffset > m_pStatus->totalSize) {
			prefix = _T("> ");
			permill = 1000;
		}
		else
			permill = wxLongLong(m_pStatus->currentOffset * 1000 / m_pStatus->totalSize).GetLo();
		m_progressLabel->SetLabel(wxString::Format(_T("%s%d.%d%%"), prefix.c_str(), permill / 10, permill % 10));
	} else {
		m_progressBar->SetValue(0);
		m_progressBar->SetRange(1);
		m_progressLabel->SetLabel(wxString::Format(_T("%s%d.%d%%"), wxString().c_str(), 0, 0));
	}
	Layout();
}

wxFileOffset CStatusLineCtrl::GetSpeed(int elapsedSeconds)
{
	if (!m_pStatus)
		return -1;

	if (elapsedSeconds <= 0)
		return -1;

	if (m_past_data_index < 9)
	{

		if (m_past_data_index == -1 || m_past_data[m_past_data_index].elapsed < elapsedSeconds)
		{
			m_past_data_index++;
			m_past_data[m_past_data_index].elapsed = elapsedSeconds;
			m_past_data[m_past_data_index].offset = m_pStatus->currentOffset - m_pStatus->startOffset;
		}
	}

	_past_data forget = {0};
	for (int i = m_past_data_index; i >= 0; i--)
	{
		if (m_past_data[i].elapsed < elapsedSeconds)
		{
			forget = m_past_data[i];
			break;
		}
	}

	return (m_pStatus->currentOffset - m_pStatus->startOffset - forget.offset) / (elapsedSeconds - forget.elapsed);
}

wxFileOffset CStatusLineCtrl::GetCurrentSpeed()
{
	if (!m_pStatus)
		return -1;

	const wxTimeSpan timeDiff( wxDateTime::UNow().Subtract(m_gcLastTimeStamp) );

	if (timeDiff.GetMilliseconds().GetLo() <= 2000)
		return m_gcLastSpeed;

	m_gcLastTimeStamp = wxDateTime::UNow();

	if (m_gcLastOffset == -1)
		m_gcLastOffset = m_pStatus->startOffset;

	const wxFileOffset fileOffsetDiff = m_pStatus->currentOffset - m_gcLastOffset;
	m_gcLastOffset = m_pStatus->currentOffset;
	m_gcLastSpeed = fileOffsetDiff * 1000 / timeDiff.GetMilliseconds().GetLo();

	return m_gcLastSpeed;
}

bool CStatusLineCtrl::Show(bool show /*=true*/)
{
	if (show)
	{
		if (!m_transferStatusTimer.IsRunning())
			m_transferStatusTimer.Start(100);
	}
	else
		m_transferStatusTimer.Stop();

	return wxWindow::Show(show);
}

void CStatusLineCtrl::SetEngineData(const t_EngineData* const pEngineData)
{
	wxASSERT(pEngineData);
	m_pEngineData = pEngineData;
}
