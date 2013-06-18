md prebuilt
cd src\sdbwinapi
build -cbeEIFZ
copy objfre_win7_x86\i386\SdbWinApi.dll ..\..\prebuilt\
copy SdbWinApi.def ..\..\prebuilt\
cd ..\..\prebuilt
dlltool --def SdbWInApi.def --dllname SdbWinApi.dll --output-lib SdbWinApi.a
cd ..\src\sdbwinusbapi
build -cbeEIFZ
copy objfre_win7_x86\i386\SdbWinUsbApi.dll ..\..\prebuilt\
