下载 https://github.com/phuslu/taskbar/archive/master.zip
然后用 vc6 打开 taskbar.dsw, 点"编译"得到 taskbar.exe
此时的 taskbar.exe 是 cmd.exe 的一个外壳
用 reshack/exesope 修改 taskbar.exe 的图标资源和字符串资源就能得到自定义的任意 console 程序的外壳了
比如 goproxy-gui.exe
