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
	status_valid_ = false;
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
	if (status_valid_ && status_.totalSize >= 0)
		m_pEngineData->pItem->SetSize(status_.totalSize);

	if (m_transferStatusTimer.IsRunning())
		m_transferStatusTimer.Stop();
}

void CStatusLineCtrl::SetTransferStatus(const CTransferStatus* pStatus)
{
	if (!pStatus) {
		if (status_valid_ && status_.totalSize >= 0)
			m_pParent->UpdateItemSize(m_pEngineData->pItem, status_.totalSize);
		status_valid_ = false;

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
		m_gcLastOffset = -1;
		m_gcLastSpeed = -1;
	}
	else {
		status_valid_ = true;
		status_ = *pStatus;

		m_lastOffset = pStatus->currentOffset;

		if (!m_transferStatusTimer.IsRunning())
			m_transferStatusTimer.Start(100);
	}
	Refresh(false);
}

void CStatusLineCtrl::OnTimer(wxTimerEvent&)
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
			m_pEngineData->pItem->GetType() == QueueItemType::File)
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
	if (status_valid_ && status_.started.IsValid())
		elapsed_seconds = wxDateTime::Now().Subtract(status_.started).GetSeconds().GetLo(); // Assume GetHi is always 0

	wxFileOffset rate;
	if (COptions::Get()->GetOptionVal(OPTION_SPEED_DISPLAY))
		rate = GetCurrentSpeed();
	else
		rate = GetSpeed(elapsed_seconds);

	CSizeFormat::_format format = static_cast<CSizeFormat::_format>(COptions::Get()->GetOptionVal(OPTION_SIZE_FORMAT));
	const wxString bytestr = CSizeFormat::Format(status_valid_ ? status_.currentOffset : 0, true, format,
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
	if (status_valid_ && status_.totalSize > 0 && elapsed_seconds && rate > 0)
	{
		wxFileOffset r = status_.totalSize - status_.currentOffset;
		left = r / rate + 1;
		if (r)
			++left;

		if (left < 0)
			left = 0;
	}

	m_elapsedLabel->SetLabel(wxTimeSpan(0, 0, elapsed_seconds).Format(_("%H:%M:%S elapsed")));
	m_remainingLabel->SetLabel(left != -1 ? wxTimeSpan(0, 0, left).Format(_("%H:%M:%S left"))
										  : _("--:--:-- left"));

	if (status_valid_ && status_.totalSize > 0) {
		m_progressBar->SetRange(status_.totalSize);
		m_progressBar->SetValue(status_.currentOffset);

		int permill;
		wxString prefix;
		if (status_.currentOffset > status_.totalSize) {
			prefix = _T("> ");
			permill = 1000;
		}
		else
			permill = wxLongLong(status_.currentOffset * 1000 / status_.totalSize).GetLo();
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
	if (!status_valid_)
		return -1;

	if (elapsedSeconds <= 0)
		return -1;

	if (m_past_data_index < 9) {
		if (m_past_data_index == -1 || m_past_data[m_past_data_index].elapsed < elapsedSeconds) {
			m_past_data_index++;
			m_past_data[m_past_data_index].elapsed = elapsedSeconds;
			m_past_data[m_past_data_index].offset = status_.currentOffset - status_.startOffset;
		}
	}

	_past_data forget;
	for (int i = m_past_data_index; i >= 0; i--) {
		if (m_past_data[i].elapsed < elapsedSeconds) {
			forget = m_past_data[i];
			break;
		}
	}

	return (status_.currentOffset - status_.startOffset - forget.offset) / (elapsedSeconds - forget.elapsed);
}

wxFileOffset CStatusLineCtrl::GetCurrentSpeed()
{
	if (!status_valid_)
		return -1;

	const wxTimeSpan timeDiff( wxDateTime::UNow().Subtract(m_gcLastTimeStamp) );

	if (timeDiff.GetMilliseconds().GetLo() <= 2000)
		return m_gcLastSpeed;

	m_gcLastTimeStamp = wxDateTime::UNow();

	if (m_gcLastOffset < 0)
		m_gcLastOffset = status_.startOffset;

	const wxFileOffset fileOffsetDiff = status_.currentOffset - m_gcLastOffset;
	m_gcLastOffset = status_.currentOffset;
	if( fileOffsetDiff >= 0 ) {
		m_gcLastSpeed = fileOffsetDiff * 1000 / timeDiff.GetMilliseconds().GetLo();
	}

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
