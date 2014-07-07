#include <filezilla.h>
#include "systemimagelist.h"
#ifdef __WXMSW__
#include "shlobj.h"

	// Once again lots of the shell stuff is missing in MinGW's headers
	#ifndef IDO_SHGIOI_LINK
		#define IDO_SHGIOI_LINK 0x0FFFFFFE
	#endif
	#ifndef SHGetIconOverlayIndex
		extern "C" int WINAPI SHGetIconOverlayIndexW(LPCWSTR pszIconPath, int iIconIndex);
		extern "C" int WINAPI SHGetIconOverlayIndexA(LPCSTR pszIconPath, int iIconIndex);
		#define SHGetIconOverlayIndex SHGetIconOverlayIndexW
	#endif
#endif
#ifndef __WXMSW__
#include "themeprovider.h"
#include <wx/rawbmp.h>
#endif
#ifdef __WXGTK__
#include <gio/gio.h>
#include <gtk/gtk.h>
#include <wx/dynlib.h>
#endif

wxImageListEx::wxImageListEx()
	: wxImageList()
{
}

wxImageListEx::wxImageListEx(int width, int height, const bool mask /*=true*/, int initialCount /*=1*/)
	: wxImageList(width, height, mask, initialCount)
{
}

#ifdef __WXMSW__
HIMAGELIST wxImageListEx::Detach()
{
	 HIMAGELIST hImageList = (HIMAGELIST)m_hImageList;
	 m_hImageList = 0;
	 return hImageList;
}
#endif

#ifndef __WXMSW__
static inline void AlphaComposite_Over_Inplace(wxAlphaPixelData::Iterator &a, wxAlphaPixelData::Iterator &b)
{
	// Alpha compositing of a single pixel, b gets composited over a
	// (well-known over operator), result stored in a.
	// All RGB and A values have range from 0 to 255, RGB values aren't
	// premultiplied by A.
	// Safe for multiple compositions.

	if (!b.Alpha())
	{
		// Nothing to do
		return;
	}

	int new_alpha = a.Alpha() + b.Alpha() - a.Alpha() * b.Alpha() / 255; // Could only get 0 if both alphas were 0, caught that already.
	a.Red() = ((int)a.Red() * (255 - b.Alpha()) * a.Alpha() / 255 + (int)b.Red() * b.Alpha()) / new_alpha;
	a.Green() = ((int)a.Green() * (255 - b.Alpha()) * a.Alpha() / 255 + (int)b.Green() * b.Alpha()) / new_alpha;
	a.Blue() = ((int)a.Blue() * (255 - b.Alpha()) * a.Alpha() / 255 + (int)b.Blue() * b.Alpha()) / new_alpha;
	a.Alpha() = new_alpha;
}

static void OverlaySymlink(wxBitmap& bmp)
{
	// This is ugly, but apparently needed so that the data is _really_ in the right internal format
	bmp = bmp.ConvertToImage();

	wxBitmap symlink;
#ifdef __WXGTK__
	symlink = wxArtProvider::GetBitmap(_T("emblem-symbolic-link"), wxART_OTHER, wxSize(bmp.GetWidth(), bmp.GetHeight()));
#endif
	if (!symlink.IsOk())
		symlink = wxArtProvider::GetBitmap(_T("ART_SYMLINK"), wxART_OTHER, wxSize(bmp.GetWidth(), bmp.GetHeight()));
	symlink = symlink.ConvertToImage();

	wxAlphaPixelData target(bmp);
	wxAlphaPixelData source(symlink);

	int sx = bmp.GetWidth();
	if (symlink.GetWidth() < sx)
		sx = symlink.GetWidth();
	int sy = bmp.GetHeight();
	if (symlink.GetHeight() < sy)
		sy = symlink.GetHeight();

	// Do some rudimentary alpha copying
	wxAlphaPixelData::Iterator t(target);
	wxAlphaPixelData::Iterator s(source);
	for (int y = 0; y < sy; y++)
	{
		s.MoveTo(source, 0, y);
		t.MoveTo(target, 0, y);
		for (int x = 0; x < sx; x++, s++, t++)
			AlphaComposite_Over_Inplace(t, s);
	}
}
#endif

CSystemImageList::CSystemImageList(int size /*=-1*/)
	: m_pImageList()
{
	if (size != -1)
		CreateSystemImageList(size);
}

bool CSystemImageList::CreateSystemImageList(int size)
{
	if (m_pImageList)
		return true;

#ifdef __WXMSW__
	SHFILEINFO shFinfo;
	wxChar buffer[MAX_PATH + 10];
	if (!GetWindowsDirectory(buffer, MAX_PATH))
#ifdef _tcscpy
		_tcscpy(buffer, _T("C:\\"));
#else
		strcpy(buffer, _T("C:\\"));
#endif

	m_pImageList = new wxImageListEx((WXHIMAGELIST)SHGetFileInfo(buffer,
							  0,
							  &shFinfo,
							  sizeof( shFinfo ),
							  SHGFI_SYSICONINDEX |
							  ((size != 16) ? SHGFI_ICON : SHGFI_SMALLICON) ));
#else
	m_pImageList = new wxImageListEx(size, size);

	wxBitmap file;
	wxBitmap folderclosed;
	wxBitmap folder;
#ifdef __WXGTK__
	file = wxArtProvider::GetBitmap(_T("unknown"),  wxART_OTHER, wxSize(size, size));
	folderclosed = wxArtProvider::GetBitmap(_T("folder"),  wxART_OTHER, wxSize(size, size));
	folder = wxArtProvider::GetBitmap(_T("folder-open"),  wxART_OTHER, wxSize(size, size));
//#elif defined(__WXMAC__)
//Get from system:
//	[[NSWorkspace sharedWorkspace] iconForFileType:NSFileTypeForHFSTypeCode(kGenericFolderIcon)]
#endif
	if (!file.IsOk())
		file = wxArtProvider::GetBitmap(_T("ART_FILE"),  wxART_OTHER, wxSize(size, size));
	if (!folderclosed.IsOk())
		folderclosed = wxArtProvider::GetBitmap(_T("ART_FOLDERCLOSED"),  wxART_OTHER, wxSize(size, size));
	if (!folder.IsOk())
		folder = wxArtProvider::GetBitmap(_T("ART_FOLDER"),  wxART_OTHER, wxSize(size, size));

	m_pImageList->Add(file);
	m_pImageList->Add(folderclosed);
	m_pImageList->Add(folder);
	OverlaySymlink(file);
	OverlaySymlink(folderclosed);
	OverlaySymlink(folder);
	m_pImageList->Add(file);
	m_pImageList->Add(folderclosed);
	m_pImageList->Add(folder);
#endif

	return true;
}

CSystemImageList::~CSystemImageList()
{
	if (!m_pImageList)
		return;

#ifdef __WXMSW__
	m_pImageList->Detach();
#endif

	delete m_pImageList;

	m_pImageList = 0;
}

#ifdef __WXGTK__

typedef enum {
    GNOME_ICON_LOOKUP_FLAGS_NONE = 0,
    GNOME_ICON_LOOKUP_FLAGS_EMBEDDING_TEXT = 1<<0,
    GNOME_ICON_LOOKUP_FLAGS_SHOW_SMALL_IMAGES_AS_THEMSELVES = 1<<1,
    GNOME_ICON_LOOKUP_FLAGS_ALLOW_SVG_AS_THEMSELVES = 1<<2
} GnomeIconLookupFlags;

typedef enum {
    GNOME_ICON_LOOKUP_RESULT_FLAGS_NONE = 0,
    GNOME_ICON_LOOKUP_RESULT_FLAGS_THUMBNAIL = 1<<0
} GnomeIconLookupResultFlags;

struct Gnome {
    Gnome():
        libGnomeVfs(_T("libgnomevfs-2.so.0"), wxDL_QUIET),
        libGnomeUi(_T("libgnomeui-2.so.0"), wxDL_QUIET),
        gnome_vfs_init((Gnome_Vfs_Init)libGnomeVfs.GetSymbol(_T("gnome_vfs_init"))),
        vfs_get_mime_type_for_name((Gnome_Vfs_Get_Mime_Type_For_Name)libGnomeVfs.GetSymbol(_T("gnome_vfs_get_mime_type_for_name"))),
        icon_lookup((Gnome_Icon_Lookup)libGnomeUi.GetSymbol(_T("gnome_icon_lookup")))
    {
        if (!gnome_vfs_init || !gnome_vfs_init())
            icon_lookup = NULL;
    }

    typedef int (*Gnome_Vfs_Init)(void);
    typedef const char * (*Gnome_Vfs_Get_Mime_Type_For_Name)(const char *filename);
    typedef char * (*Gnome_Icon_Lookup)(GtkIconTheme *icon_theme,
                                        const char *file_uri,
                                        void/*GnomeThumbnailFactory*/ *thumbnail_factory,
                                        const char *custom_icon,
                                        void/*GnomeVFSFileInfo*/ *file_info,
                                        const char *mime_type,
                                        GnomeIconLookupFlags flags,
                                        GnomeIconLookupResultFlags *result);
    wxDynamicLibrary libGnomeVfs;
    wxDynamicLibrary libGnomeUi;
    Gnome_Vfs_Init gnome_vfs_init;
    Gnome_Vfs_Get_Mime_Type_For_Name vfs_get_mime_type_for_name;
    Gnome_Icon_Lookup icon_lookup;
} static gnome;

static wxBitmap GetNativeFileIcon(const wxString & fileName, const wxString & ext)
{
	wxString iconName;
	if (gnome.icon_lookup)
	{
		GtkIconTheme *theme = gtk_icon_theme_get_default();
		const wxCharBuffer name = fileName.ToUTF8();
		const char * mimetype = gnome.vfs_get_mime_type_for_name(name.data());
		char * res = gnome.icon_lookup(theme, name.data(), NULL, NULL, NULL, mimetype, GNOME_ICON_LOOKUP_FLAGS_NONE, NULL);
		if (res)
			iconName = wxString::FromUTF8(res);
		g_free(res);
	}
	else
	{
		GFile * file = g_file_new_for_path(fileName.ToUTF8().data());
        GFileInfo * fileInfo = file ? g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_ICON, G_FILE_QUERY_INFO_NONE, NULL, NULL) : NULL;
        GIcon * fileIcon = fileInfo ? g_file_info_get_icon(fileInfo) : NULL;
        const gchar * const * iconNames = fileIcon ? g_themed_icon_get_names(G_THEMED_ICON(fileIcon)) : NULL;
		if (iconNames)
			iconName = wxString::FromUTF8(iconNames[0]);
        if (fileInfo)
            g_object_unref(fileInfo);
        if (file)
            g_object_unref(file);
	}

	wxSize iconSize = CThemeProvider::GetIconSize(iconSizeSmall);
    if (iconName.IsEmpty())
        return wxBitmap();
    if (iconName.GetChar(0) == '/')
		return wxImage(iconName).Rescale(iconSize.GetWidth(), iconSize.GetHeight());
    return wxArtProvider::GetBitmap(iconName, wxART_OTHER, CThemeProvider::GetIconSize(iconSizeSmall));
}

#else

static wxBitmap GetNativeFileIcon(const wxString & /*fileName*/, const wxString & /*ext*/)
{
    return wxBitmap();
}

#endif

// This function converts to the right size with the given background colour
static wxBitmap PrepareIcon(wxIcon icon, wxSize size)
{
	if (icon.GetWidth() == size.GetWidth() && icon.GetHeight() == size.GetHeight())
		return icon;
	wxBitmap bmp;
	bmp.CopyFromIcon(icon);
	return bmp.ConvertToImage().Rescale(size.GetWidth(), size.GetHeight());
}

static wxBitmap GetFileIcon(const wxString & /*fileName*/, const wxString & ext)
{
	wxFileType *pType = wxTheMimeTypesManager->GetFileTypeFromExtension(ext);
	if (!pType)
		return wxBitmap();

	wxBitmap bmp;
	wxIconLocation loc;
	if (pType->GetIcon(&loc) && loc.IsOk())
	{
		wxLogNull nul;
		wxIcon newIcon(loc);

		if (newIcon.Ok())
			bmp = PrepareIcon(newIcon, CThemeProvider::GetIconSize(iconSizeSmall));
	}
	delete pType;

	return bmp;
}

int CSystemImageList::GetIconIndex(iconType type, const wxString& fileName /*=_T("")*/, bool physical /*=true*/, bool symlink /*=false*/)
{
	if (!m_pImageList)
		return -1;

#ifdef __WXMSW__
	if (fileName.empty())
		physical = false;

	SHFILEINFO shFinfo;
	memset(&shFinfo, 0, sizeof(SHFILEINFO));
	if (SHGetFileInfo(!fileName.empty() ? fileName : _T("{B97D3074-1830-4b4a-9D8A-17A38B074052}"),
		(type != iconType::file) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL,
		&shFinfo,
		sizeof(SHFILEINFO),
		SHGFI_ICON | ((type == iconType::opened_dir) ? SHGFI_OPENICON : 0) | ((physical) ? 0 : SHGFI_USEFILEATTRIBUTES) ) )
	{
		int icon = shFinfo.iIcon;
		// we only need the index from the system image list
		DestroyIcon(shFinfo.hIcon);
		return icon;
	}
#else
	int icon;
	switch (type)
	{
	case iconType::file:
	default:
		icon = symlink ? 3 : 0;
		break;
	case iconType::dir:
		return symlink ? 4 : 1;
	case iconType::opened_dir:
		return symlink ? 5 : 2;
	}

	wxFileName fn(fileName);
	wxString ext = fn.GetExt();
	if (ext.empty())
		return icon;

	if (symlink)
	{
		auto cacheIter = m_iconCache.find(ext);
		if (cacheIter != m_iconCache.end())
			return cacheIter->second;
	}
	else
	{
		auto cacheIter = m_iconSymlinkCache.find(ext);
		if (cacheIter != m_iconSymlinkCache.end())
			return cacheIter->second;
	}

    wxBitmap bmp = GetNativeFileIcon(fileName, ext);
    if (!bmp.IsOk())
        bmp = GetFileIcon(fileName, ext);
	if (bmp.IsOk()) {
		if (symlink)
			OverlaySymlink(bmp);
		int index = m_pImageList->Add(bmp);
		if (index > 0)
			icon = index;
	}

	if (symlink)
		m_iconCache[ext] = icon;
	else
		m_iconSymlinkCache[ext] = icon;
	return icon;
#endif
	return -1;
}

#ifdef __WXMSW__
int CSystemImageList::GetLinkOverlayIndex()
{
	static int overlay = -1;
	if (overlay == -1)
	{
		overlay = SHGetIconOverlayIndex(0, IDO_SHGIOI_LINK);
		if (overlay < 0)
			overlay = 0;
	}

	return overlay;
}
#endif
