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

// �֐��v���g�^�C�v
DWORD GetWindowProcessIdFromName(const wchar_t* pTitle);
std::vector<DWORD> GetProcessIdFromExeName(const wchar_t* pExeName);
DWORD GetWindowProcessId(HWND hwnd);
bool SetApplicationVolume(const std::vector<DWORD>& processIds, float leftVolume, float rightVolume);

int main() {
    std::locale::global(std::locale("ja_JP.UTF-8"));

    //// �v���Z�X�����擾
    //DWORD processId = GetWindowProcessIdFromName(L"Minecraft* 1.21 - �V���O���v���C");
    //if (processId == NULL) {
    //    std::wcerr << L"�E�B���h�E����v���Z�XID���擾�ł��܂���ł����B" << std::endl;
    //    return 1;
    //}

	// �v���Z�X������v���Z�XID���擾
    std::vector<DWORD> processIds = GetProcessIdFromExeName(L"Discord.exe");
	if (processIds.empty()) {
		std::wcerr << L"�v���Z�XID���擾�ł��܂���ł����B" << std::endl;
		return 1;
	}

    // ���ʂ�volume%�ɐݒ�
	float leftVolume = 0.0f;
	float rightVolume = 1.0f;
    if (!SetApplicationVolume(processIds, leftVolume, rightVolume)) {
        std::wcerr << L"���ʂ�ύX�ł��܂���ł����B" << std::endl;
        return 1;
    }

	std::wcout << L"���ʂ���" << (leftVolume * 100) << L"%�A�E" << (rightVolume * 100) << L"%�ɐݒ肵�܂����B" << std::endl;
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
		std::wcerr << L"COM�̏������Ɏ��s���܂����B" << std::endl;
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
		std::wcerr << L"�f�o�C�X�񋓎q�̍쐬�Ɏ��s���܂����B" << std::endl;
		goto cleanup;
    }

    hr = deviceEnumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &defaultDevice);
    if (FAILED(hr))
    {
		std::wcerr << L"�f�t�H���g�̃I�[�f�B�I�f�o�C�X�̎擾�Ɏ��s���܂����B" << std::endl;
		goto cleanup;
    }

    hr = defaultDevice->Activate(__uuidof(IAudioSessionManager2), CLSCTX_INPROC_SERVER, NULL, (void**)&sessionManager);
    if (FAILED(hr))
    {
		std::wcerr << L"�Z�b�V�����}�l�[�W���̍쐬�Ɏ��s���܂����B" << std::endl;
		goto cleanup;
    }

    hr = sessionManager->GetSessionEnumerator(&sessionEnumerator);
    if (FAILED(hr))
    {
		std::wcerr << L"�Z�b�V�����񋓎q�̍쐬�Ɏ��s���܂����B" << std::endl;
		goto cleanup;
    }

    int sessionCount;
    hr = sessionEnumerator->GetCount(&sessionCount);
    if (FAILED(hr))
    {
		std::wcerr << L"�Z�b�V�������̎擾�Ɏ��s���܂����B" << std::endl;
		goto cleanup;
    }

    for (int i = 0; i < sessionCount; i++) {
        IAudioSessionControl* sessionControl = NULL;
        IAudioSessionControl2* sessionControl2 = NULL;

        hr = sessionEnumerator->GetSession(i, &sessionControl);
        if (FAILED(hr))
        {
			std::wcerr << L"�Z�b�V�����̎擾�Ɏ��s���܂����B" << std::endl;
			continue;
        }

        hr = sessionControl->QueryInterface(__uuidof(IAudioSessionControl2), (void**)&sessionControl2);
        sessionControl->Release();
        if (FAILED(hr))
        {
			std::wcerr << L"�Z�b�V�����R���g���[���̎擾�Ɏ��s���܂����B" << std::endl;
			continue;
        }

        DWORD processId;
        hr = sessionControl2->GetProcessId(&processId);
        if (FAILED(hr)) {
			std::wcerr << L"�v���Z�XID�̎擾�Ɏ��s���܂����B" << std::endl;
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
                    std::wcerr << L"�`�����l�����̎擾�Ɏ��s���܂����B" << std::endl;
                }
                else
                {
                    // ��
                    if (channelCount >= 1)
                    {
						hr = audioVolume->SetChannelVolume(0, leftVolume, NULL);
						if (FAILED(hr))
						{
							std::wcerr << L"���̉��ʂ̐ݒ�Ɏ��s���܂����B" << std::endl;
							break;
						}
                    }
							
					// �E
                    if (channelCount >= 2)
                    {
                        hr = audioVolume->SetChannelVolume(1, rightVolume, NULL);
                        if (FAILED(hr))
                        {
                            std::wcerr << L"�E�̉��ʂ̐ݒ�Ɏ��s���܂����B" << std::endl;
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
