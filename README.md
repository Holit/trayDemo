# trayDemo

用滚轮调整系统音量的小工具。

## 用法

启动此程序，然后就可以把鼠标移动到Speaker图标上，滚轮即可增加、减少音量。

## 原理

程序通过获取**用户提示通知区域**的矩形位置，并获取内部各图标的标题，匹配Speaker图标位置。然后程序添加Hook，对MOUSE_LL消息进行挂钩，利用PostMessage通知应用程序变更系统音量。

由于waveOutSetVolume总是失败（变更波形而不是变更Master音量），因此程序发送VK_VOLUP等按键来完成音量控制操作。

## 已知问题

* 由于仅在程序启动时获取相关位置，因此如果出现分辨率变更、Tray刷新重排，会导致无法跟随Speaker图标变动位置。