#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <audiopolicy.h>
#include <string>
#include <iostream>
#include <psapi.h>
#include <tlhelp32.h>
#include <tchar.h>
#include <vector>

// 関数プロトタイプ
DWORD GetWindowProcessIdFromName(const wchar_t* pTitle);
std::vector<DWORD> GetProcessIdFromExeName(const wchar_t* pExeName);
DWORD GetWindowProcessId(HWND hwnd);
bool SetApplicationVolume(const std::vector<DWORD>& processIds, float leftVolume, float rightVolume);

int main() {
    std::locale::global(std::locale("ja_JP.UTF-8"));

    //// プロセス名を取得
    //DWORD processId = GetWindowProcessIdFromName(L"Minecraft* 1.21 - シングルプレイ");
    //if (processId == NULL) {
    //    std::wcerr << L"ウィンドウからプロセスIDを取得できませんでした。" << std::endl;
    //    return 1;
    //}

	// プロセス名からプロセスIDを取得
    std::vector<DWORD> processIds = GetProcessIdFromExeName(L"Discord.exe");
	if (processIds.empty()) {
		std::wcerr << L"プロセスIDを取得できませんでした。" << std::endl;
		return 1;
	}

    // 音量をvolume%に設定
	float leftVolume = 0.0f;
	float rightVolume = 1.0f;
    if (!SetApplicationVolume(processIds, leftVolume, rightVolume)) {
        std::wcerr << L"音量を変更できませんでした。" << std::endl;
        return 1;
    }

	std::wcout << L"音量を左" << (leftVolume * 100) << L"%、右" << (rightVolume * 100) << L"%に設定しました。" << std::endl;
    return 0;
}

DWORD GetWindowProcessIdFromName(const wchar_t* pTitle)
{
    DWORD processId = 0;
    HWND hwnd = FindWindow(NULL, pTitle);
    if (hwnd) {
        GetWindowThreadProcessId(hwnd, &processId);
    }
    return processId;
}

std::vector<DWORD> GetProcessIdFromExeName(const wchar_t* pExeName)
{
	std::vector<DWORD> processIds;
	HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hSnap == INVALID_HANDLE_VALUE) return processIds;

	PROCESSENTRY32 pe;
	pe.dwSize = sizeof(pe);
	if (Process32First(hSnap, &pe)) {
		do {
			if (wcscmp(pe.szExeFile, pExeName) == 0) {
                processIds.push_back(pe.th32ProcessID);
				//break;
			}
		} while (Process32Next(hSnap, &pe));
	}
	CloseHandle(hSnap);
	return processIds;
}

DWORD GetWindowProcessId(HWND hwnd) {
    DWORD processId;
    GetWindowThreadProcessId(hwnd, &processId);

    HANDLE processHandle = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, processId);
    if (!processHandle) return NULL;
    CloseHandle(processHandle);

	return processId;
}

bool SetApplicationVolume(const std::vector<DWORD>& processIds, float leftVolume, float rightVolume) {
    if (FAILED(CoInitialize(NULL)))
    {
		std::wcerr << L"COMの初期化に失敗しました。" << std::endl;
		return false;
    }

    IMMDeviceEnumerator* deviceEnumerator = NULL;
    IMMDevice* defaultDevice = NULL;
    IAudioSessionManager2* sessionManager = NULL;
    IAudioSessionEnumerator* sessionEnumerator = NULL;

    HRESULT hr = CoCreateInstance(
        __uuidof(MMDeviceEnumerator), NULL, CLSCTX_INPROC_SERVER,
        __uuidof(IMMDeviceEnumerator), (void**)&deviceEnumerator);
    if (FAILED(hr))
    {
		std::wcerr << L"デバイス列挙子の作成に失敗しました。" << std::endl;
		goto cleanup;
    }

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
    if (FAILED(hr))
    {
		std::wcerr << L"デフォルトのオーディオデバイスの取得に失敗しました。" << std::endl;
		goto cleanup;
    }

    hr = defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, NULL, (void**)&sessionManager);
    if (FAILED(hr))
    {
		std::wcerr << L"セッションマネージャの作成に失敗しました。" << std::endl;
		goto cleanup;
    }

    hr = sessionManager->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr))
    {
		std::wcerr << L"セッション列挙子の作成に失敗しました。" << std::endl;
		goto cleanup;
    }

    int sessionCount;
    hr = sessionEnumerator->GetCount(&sessionCount);
    if (FAILED(hr))
    {
		std::wcerr << L"セッション数の取得に失敗しました。" << std::endl;
		goto cleanup;
    }

    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* sessionControl = NULL;
        IAudioSessionControl2* sessionControl2 = NULL;

        hr = sessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr))
        {
			std::wcerr << L"セッションの取得に失敗しました。" << std::endl;
			continue;
        }

        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        sessionControl->Release();
        if (FAILED(hr))
        {
			std::wcerr << L"セッションコントロールの取得に失敗しました。" << std::endl;
			continue;
        }

        DWORD processId;
        hr = sessionControl2->GetProcessId(&processId);
        if (FAILED(hr)) {
			std::wcerr << L"プロセスIDの取得に失敗しました。" << std::endl;
            sessionControl2->Release();
            continue;
        }

        if (std::find(processIds.begin(), processIds.end(), processId) != processIds.end()) {
            IChannelAudioVolume* audioVolume = NULL;
            hr = sessionControl2->QueryInterface(__uuidof(IChannelAudioVolume), (void**)&audioVolume);
            if (SUCCEEDED(hr)) {
				UINT channelCount = 0;
                hr = audioVolume->GetChannelCount(&channelCount);
                if (FAILED(hr))
                {
                    std::wcerr << L"チャンネル数の取得に失敗しました。" << std::endl;
                }
                else
                {
                    // 左
                    if (channelCount >= 1)
                    {
						hr = audioVolume->SetChannelVolume(0, leftVolume, NULL);
						if (FAILED(hr))
						{
							std::wcerr << L"左の音量の設定に失敗しました。" << std::endl;
							break;
						}
                    }
							
					// 右
                    if (channelCount >= 2)
                    {
                        hr = audioVolume->SetChannelVolume(1, rightVolume, NULL);
                        if (FAILED(hr))
                        {
                            std::wcerr << L"右の音量の設定に失敗しました。" << std::endl;
                            break;
                        }
                    }
                }
                audioVolume->Release();
                sessionControl2->Release();
                goto cleanup_success;
            }
        }
        sessionControl2->Release();
    }

cleanup:
    if (sessionEnumerator) sessionEnumerator->Release();
    if (sessionManager) sessionManager->Release();
    if (defaultDevice) defaultDevice->Release();
    if (deviceEnumerator) deviceEnumerator->Release();
    CoUninitialize();
    return false;

cleanup_success:
    if (sessionEnumerator) sessionEnumerator->Release();
    if (sessionManager) sessionManager->Release();
    if (defaultDevice) defaultDevice->Release();
    if (deviceEnumerator) deviceEnumerator->Release();
    CoUninitialize();
    return true;
}
