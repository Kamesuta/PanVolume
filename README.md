# オーディオのPan(左右のバランス)を変更するサンプルコード

Windows API (Core Audio API) を使って、オーディオのPan(左右のバランス)を変更するサンプルコードです。

アプリごとに左右のバランス(パン)を変更できます。  
(これにより例えば、Discordを同時起動して、Discordが左から鳴り、Discord PTBが中央から、Discord Canaryが右から鳴るように設定して、同時VCを使い分けたりすることができます。)

このサンプルコードでは Discord の音量を右から鳴るように変更しています。

```cpp
// 音量をvolume%に設定
float leftVolume = 0.0f;
float rightVolume = 1.0f;
if (!SetApplicationVolume(processIds, leftVolume, rightVolume)) {
    std::wcerr << L"音量を変更できませんでした。" << std::endl;
    return 1;
}

std::wcout << L"音量を左" << (leftVolume * 100) << L"%、右" << (rightVolume * 100) << L"%に設定しました。" << std::endl;
```