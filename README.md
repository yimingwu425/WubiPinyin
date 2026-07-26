# 五笔·拼音

基于 Rime 输入法引擎和 rime/weasel 的 GPLv3 派生项目。首版提供简体中文 86 五笔与全拼在同一 composition 中的自动竞争输入。

## MVP 平台边界

- 仅支持 Windows 11 x64，安装程序会拒绝 Windows 10、x86 和 ARM64。
- 安装包同时包含 x86 和 x64 TIP DLL；x64 Broker、Deployer 和 WinUI 3 设置程序只在 x64 上运行。
- `WubiPinyinSetup.exe` 故意保持为 32 位注册引导器，用于注册两种 TIP 位数；它不是设置程序。
- 学习、设置和用户词库仅保存在本机 `%AppData%\WubiPinyin`。不提供云同步、自动更新、双拼、模糊音或繁体输入。

## 安装

运行未签名的个人安装包后，开始菜单中的“设置”会打开 x64 设置程序。卸载时默认保留用户词库和设置；只有明确确认后才删除它们。

使用輸入法
----------

選取輸入法指示器菜單裏的【中】字樣圖標，開始用五笔·拼音寫字。

可通過 <kbd>Ctrl+Shift+A</kbd>、<kbd>Ctrl+Shift+W</kbd>、<kbd>Ctrl+Shift+P</kbd> 臨時切換自動、五笔與拼音路由。

定製輸入法
----------

通過開始菜單中的五笔·拼音設定工具管理輸入、外觀、用戶詞庫與學習資料。

用戶詞庫和設定位於 `%AppData%\WubiPinyin`；設定程式的「關於」頁會顯示資料位置。手工詞條由 Broker 寫入並在維護流程中更新 Rime 詞典。

碼表來源、鎖定版本和歸屬說明見 [WubiPinyinData/THIRD_PARTY_NOTICES.md](WubiPinyinData/THIRD_PARTY_NOTICES.md) 與 [WubiPinyinData/sources.lock.json](WubiPinyinData/sources.lock.json)。

致謝
----

### 輸入方案設計：

  * 【朙月拼音】系列及【八股文】詞典
    - 部分數據來源於 CC-CEDICT、Android 拼音、新酷音、opencc 等開源項目
    - 維護者：佛振、瑾昀
  * 【注音／地球拼音】
    - 維護者：佛振、瑾昀
  * 【倉頡五代】
    - 發明人：朱邦復先生
    - 碼表源自 www.chinesecj.com
    - 構詞碼表作者：惜緣

### 程序設計：

  * [佛振](https://github.com/lotem)
  * [鄒旭](https://github.com/zouxu09)
  * [Xiangyan Sun](https://github.com/wishstudio)
  * [Prcuvu](https://github.com/Prcuvu)
  * [nameoverflow](https://github.com/nameoverflow)
  * [fxliang](https://github.com/fxliang)
  * [Azuk 443](https://github.com/determ1ne)

  查看更多 [代碼貢獻者](https://github.com/rime/weasel/graphs/contributors)

### 美術：

  * 圖標設計／[Patricivs](https://github.com/Patricivs)
  * 配色方案／Aben、P1461、Patricivs、skoj、佛振、五磅兔

### 本品引用了以下開源軟件：

  * [Boost C++ Libraries](http://www.boost.org/) (Boost Software License)
  * [curl](https://curl.haxx.se/) (MIT/X derivate license)
  * [google-glog](https://github.com/google/glog) (BSD 3-Clause License)
  * [Google Test](https://github.com/google/googletest) (BSD 3-Clause License)
  * [LevelDB](https://github.com/google/leveldb) (BSD 3-Clause License)
  * [librime](https://github.com/rime/librime) (BSD 3-Clause License)
  * [marisa-trie](https://github.com/s-yata/marisa-trie) (BSD 2-Clause License, LGPL 2.1)
  * [OpenCC / 開放中文轉換](https://github.com/BYVoid/OpenCC) (Apache License 2.0)
  * [plum](https://github.com/rime/plum) (GNU Lesser General Public License v3.0)
  * [yaml-cpp](https://github.com/jbeder/yaml-cpp) (MIT License)
  * [7-Zip](https://www.7-zip.org) (GNU LGPLv2.1+ with unRAR restriction)

問題與反饋
----------

發現程序有 bug，請到 GitHub 反饋
<https://github.com/rime/weasel/issues>

歡迎提交 pull request
<https://github.com/rime/weasel/pulls>

Rime 輸入法（不限於 Windows 平臺）功能、使用方法與配置相關的問題，請反饋到
<https://github.com/rime/home/issues>

聯繫方式
--------

技術交流，歡迎光臨 [Rime 代碼之家](https://github.com/rime/home)，或致信 Rime 開發者 <rimeime@gmail.com>

謝謝！
