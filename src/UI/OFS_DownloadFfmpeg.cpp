#include "OFS_DownloadFfmpeg.h"
#include "OFS_Util.h"
#include "SDL_thread.h"

#include <sstream>
#include <filesystem>

#ifdef WIN32
    //#define WIN32_LEAN_AND_MEAN 
    #include <Windows.h>
    #include <urlmon.h>
    #pragma comment(lib, "urlmon.lib")

    class DownloadStatusCallback : public IBindStatusCallback
    {
    public:
        float Progress = 0.f;
        bool Stopped = false;

        HRESULT __stdcall QueryInterface(const IID &,void **) { 
            return E_NOINTERFACE;
        }
        ULONG STDMETHODCALLTYPE AddRef(void) { 
            return 1;
        }
        ULONG STDMETHODCALLTYPE Release(void) {
            return 1;
        }
        HRESULT STDMETHODCALLTYPE OnStartBinding(DWORD dwReserved, IBinding *pib) {
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE GetPriority(LONG *pnPriority) {
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE OnLowResource(DWORD reserved) {
            return S_OK;
        }
        virtual HRESULT STDMETHODCALLTYPE OnStopBinding(HRESULT hresult, LPCWSTR szError) {
            Stopped = true;
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE GetBindInfo(DWORD *grfBINDF, BINDINFO *pbindinfo) {
            return E_NOTIMPL;
        }
        virtual HRESULT STDMETHODCALLTYPE OnDataAvailable(DWORD grfBSCF, DWORD dwSize, FORMATETC *pformatetc, STGMEDIUM *pstgmed) {
            return E_NOTIMPL;
        }        
        virtual HRESULT STDMETHODCALLTYPE OnObjectAvailable(REFIID riid, IUnknown *punk) {
            return E_NOTIMPL;
        }

        virtual HRESULT __stdcall OnProgress(ULONG ulProgress, ULONG ulProgressMax, ULONG ulStatusCode, LPCWSTR szStatusText)
        {
            Progress = (double)ulProgress/(double)ulProgressMax;
            return S_OK;
        }
    };

    static DownloadStatusCallback cb;

    static bool ExecuteSynchronous(const wchar_t* program, const wchar_t* params) noexcept
    {
        bool succ = false;
        SHELLEXECUTEINFOW ShExecInfo = {0};
        ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
        ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
        ShExecInfo.hwnd = NULL;
        ShExecInfo.lpVerb = NULL;
        ShExecInfo.lpFile = program;        
        ShExecInfo.lpParameters = params;   
        ShExecInfo.lpDirectory = NULL;
        ShExecInfo.nShow = SW_HIDE;
        ShExecInfo.hInstApp = NULL; 
        ShellExecuteExW(&ShExecInfo);
        WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
        
        DWORD resultCode;
        if(!GetExitCodeProcess(ShExecInfo.hProcess, &resultCode)) {
            succ = false;
        }
        else {
            succ = resultCode == 0;
        }
        CloseHandle(ShExecInfo.hProcess);
        return succ;
    }
#endif

ImGuiID OFS_DownloadFfmpeg::ModalId = 0;
bool OFS_DownloadFfmpeg::FfmpegMissing = false;


void OFS_DownloadFfmpeg::DownloadFfmpegModal() noexcept
{
#ifdef WIN32
    static bool DownloadInProgress = false;
    static bool ExtractFailed = false;
    static auto ZipExists = Util::FileExists((Util::Basepath() / "ffmpeg.zip").u8string());

    bool isOpen = true;
    if(ImGui::BeginPopupModal(OFS_DownloadFfmpeg::ModalText, DownloadInProgress ? NULL : &isOpen, ImGuiWindowFlags_AlwaysAutoResize)) {
        if(DownloadInProgress) ImGui::ProgressBar(cb.Progress);
        if(!ZipExists && !DownloadInProgress) {
            ImGui::TextUnformatted("ffmpeg.exe was not found.\nDo you want to download it?");
            if(ImGui::Button("Yes", ImVec2(-1.f, 0.f))) {
                auto dlThread = [](void* data) -> int {
                    auto path = Util::Basepath();
                    auto downloadPath = (path / "ffmpeg.zip").u8string();
                    URLDownloadToFile(NULL, "https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip",
                        downloadPath.c_str(), 0, &cb);
                    return 0;
                };
                DownloadInProgress = true;
                SDL_DetachThread(SDL_CreateThread(dlThread, "downloadFfmpeg", NULL));
            }
        }

        if(cb.Stopped && (cb.Progress < 1.f || std::isnan(cb.Progress))) {
            Util::MessageBoxAlert("Error", "Failed to download from https://www.gyan.dev/ffmpeg/builds/ffmpeg-release-essentials.zip");
            FfmpegMissing = false;
            ImGui::CloseCurrentPopup();
        }

        if(!ExtractFailed && (ZipExists || cb.Stopped && cb.Progress >= 1.f)) {
            DownloadInProgress = false;
            ZipExists = true;
            std::wstringstream ss;
            auto path = Util::Basepath();
            auto downloadPath = (path / "ffmpeg.zip");

            ss << L"-xvf ";
            ss << L'"' << downloadPath.wstring() << L'"';
            ss << L" --strip-components 2  **/ffmpeg.exe";

            auto params = ss.str();
            ExtractFailed = !ExecuteSynchronous(L"tar.exe", params.c_str());

            if(!ExtractFailed) {
                FfmpegMissing = false;
                ImGui::CloseCurrentPopup();
                Util::MessageBoxAlert("Done.", "ffmpeg.exe was successfully extracted.");
                std::error_code ec;
                std::filesystem::remove(downloadPath, ec);
            }
        }
        else if(ExtractFailed) {
            ImGui::TextColored((ImColor)IM_COL32(255, 0, 0, 255), "Failed to extract ffmpeg.exe.");
            ImGui::TextUnformatted("Extracting uses tar.exe.\nIt will fail if it's not on your system.");
            ImGui::TextDisabled("tar.exe has been part of Windows 10 since build 17063");
        }

        ImGui::EndPopup();
    }

    if(!isOpen) {
        FfmpegMissing = false;
    }
#endif // WIN32
}