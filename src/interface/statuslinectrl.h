#ifndef __STATUSLINECTRL_H__
#define __STATUSLINECTRL_H__

class CQueueView;
class CStatusLineCtrl : public wxWindow
{
public:
	CStatusLineCtrl(CQueueView* pParent, const t_EngineData* const pEngineData, const wxRect& initialPosition);
	~CStatusLineCtrl();

	const CFileItem* GetItem() const { return m_pEngineData->pItem; }

	void SetEngineData(const t_EngineData* const pEngineData);

	void SetTransferStatus(const CTransferStatus* pStatus);
	wxLongLong GetLastOffset() const { return m_pStatus ? m_pStatus->currentOffset : m_lastOffset; }
	wxLongLong GetTotalSize() const { return m_pStatus ? m_pStatus->totalSize : -1; }
	wxFileOffset GetSpeed(int elapsedSeconds);
	wxFileOffset GetCurrentSpeed();

	virtual bool Show(bool show = true);

protected:
	CQueueView* m_pParent;
	const t_EngineData* m_pEngineData;
	CTransferStatus* m_pStatus;

	wxString m_statusText;
	wxTimer m_transferStatusTimer;

	bool m_madeProgress;

	wxLongLong m_lastOffset; // Stores the last transfer offset so that the total queue size can be accurately calculated.

	// This is used by GetSpeed to forget about the first 10 seconds on longer transfers
	// since at the very start the speed is hardly accurate (e.g. due to TCP slow start)
	struct _past_data
	{
		int elapsed;
		wxFileOffset offset;
	} m_past_data[10];
	int m_past_data_index;

	//Used by getCurrentSpeed
	wxDateTime m_gcLastTimeStamp;
	wxFileOffset m_gcLastOffset;
	wxFileOffset m_gcLastSpeed;

	//Used to avoid excessive redraws
	wxStaticText * m_bytes_and_rateLabel;
	wxStaticText * m_elapsedLabel;
	wxStaticText * m_remainingLabel;
	wxGauge * m_progressBar;
	wxStaticText * m_progressLabel;

	DECLARE_EVENT_TABLE()
	void OnTimer(wxTimerEvent& event);
};

#endif // __STATUSLINECTRL_H__
