#ifndef __QUEUEVIEW_H__
#define __QUEUEVIEW_H__

#define PRIORITY_COUNT 5
enum QueuePriority
{
	priority_lowest = 0,
	priority_low = 1,
	priority_normal = 2,
	priority_high = 3,
	priority_highest = 4,
};

enum QueueItemType
{
	QueueItemType_Server,
	QueueItemType_File,
	QueueItemType_Folder
};

enum ItemState
{
	ItemState_Wait,
	ItemState_Active,
	ItemState_Complete,
	ItemState_Error
};

class CQueueItem
{
public:
	virtual ~CQueueItem();

	void Expand(bool recursive);
	void Collapse(bool recursive);
	bool IsExpanded() const;

	virtual void SetPriority(enum QueuePriority priority);

	void AddChild(CQueueItem* pItem);
	unsigned int GetVisibleCount() const;
	CQueueItem* GetChild(unsigned int item);
	CQueueItem* GetParent() { return m_parent; }
	virtual bool RemoveChild(CQueueItem* pItem); // Removes a child item with is somewhere in the tree of children
	CQueueItem* GetTopLevelItem();

	virtual enum QueueItemType GetType() const = 0;

protected:
	CQueueItem();
	wxString GetIndent();

	CQueueItem* m_parent;

	std::vector<CQueueItem*> m_children;
	int m_visibleOffspring; // Visible offspring over all expanded sublevels
	bool m_expanded;
	wxString m_indent;
};

class CFileItem;
class CServerItem : public CQueueItem
{
public:
	CServerItem(const CServer& server);
	virtual ~CServerItem();

	const CServer& GetServer() const;
	wxString GetName() const;

	virtual enum QueueItemType GetType() const { return QueueItemType_Server; }

	void AddFileItemToList(CFileItem* pItem);

	CFileItem* GetIdleChild(bool immadiateOnly);
	virtual bool RemoveChild(CQueueItem* pItem); // Removes a child item with is somewhere in the tree of children

	void QueueImmediateFiles();

	int m_activeCount;

protected:
	CServer m_server;

	// array of item lists, sorted by priority. Used by scheduler to find
	// next file to transfer
	// First index specifies whether the item is queued (0) or immediate (1)
	std::list<CFileItem*> m_fileList[2][PRIORITY_COUNT];
};

class CFolderItem : public CQueueItem
{
public:

	virtual enum QueueItemType GetType() const { return QueueItemType_Folder; }

protected:
};

class CFileItem : public CQueueItem
{
public:
	CFileItem(CServerItem* parent, bool queued, bool download, const wxString& localFile,
			const wxString& remoteFile, const CServerPath& remotePath, wxLongLong size);
	CFileItem(CFolderItem* parent, bool queued, bool download, const wxString& localFile,
			const wxString& remoteFile, const CServerPath& remotePath, wxLongLong size);
	virtual ~CFileItem();

	virtual void SetPriority(enum QueuePriority priority);
	enum QueuePriority GetPriority() const;

	wxString GetLocalFile() const { return m_localFile; }
	wxString GetRemoteFile() const { return m_remoteFile; }
	CServerPath GetRemotePath() const { return m_remotePath; }
	wxLongLong GetSize() const { return m_size; }
	bool Download() const { return m_download; }
	bool Queued() const { return m_queued; }

	wxString GetIndent() { return m_indent; }

	enum ItemState GetItemState() const;
	void SetItemState(enum ItemState itemState);

	virtual enum QueueItemType GetType() const { return QueueItemType_File; }

	bool IsActive() const { return m_active; }
	void SetActive(bool active);

	bool m_queued;
	int m_errorCount;

protected:
	enum QueuePriority m_priority;
	enum ItemState m_itemState;

	bool m_download;
	wxString m_localFile;
	wxString m_remoteFile;
	CServerPath m_remotePath;
	wxLongLong m_size;
	bool m_active;
};

class CMainFrame;
class CQueueView : public wxListCtrl
{
public:
	CQueueView(wxWindow* parent, wxWindowID id, CMainFrame* pMainFrame);
	virtual ~CQueueView();
	
	bool QueueFile(bool queueOnly, bool download, const wxString& localFile, const wxString& remoteFile,
				const CServerPath& remotePath, const CServer& server, wxLongLong size);
	bool QueueFolder(bool queueOnly, bool download, const wxString& localPath, const CServerPath& remotePath, const CServer& server);
	
	bool IsEmpty() const;
	int IsActive() const { return m_activeMode; }
	bool SetActive(bool active = true);
	bool Quit();

protected:
	bool TryStartNextTransfer();

	struct t_EngineData
	{
		CFileZillaEngine* pEngine;
		bool active;

		enum EngineDataState
		{
			none,
			cancel,
			disconnect,
			connect,
			transfer
		} state;
		
		CFileItem* pItem;
		CServer lastServer;
	};
	std::vector<t_EngineData> m_engineData;

	virtual wxString OnGetItemText(long item, long column) const;
	virtual int OnGetItemImage(long item) const;

	CQueueItem* GetQueueItem(unsigned int item);
	CServerItem* GetServerItem(const CServer& server);

	void ProcessReply(t_EngineData& engineData, COperationNotification* pNotification);
	void SendNextCommand(t_EngineData& engineData);
	void ResetEngine(t_EngineData& data);
	void RemoveItem(CQueueItem* item);
	void CheckQueueState();

	std::vector<CServerItem*> m_serverList;

	int m_itemCount;
	int m_activeCount;
	int m_activeMode; // 0 inactive, 1 only immediate transfers, 2 all
	bool m_quit;

	CMainFrame* m_pMainFrame;

	DECLARE_EVENT_TABLE();

	void OnEngineEvent(wxEvent &event);
};

#endif
