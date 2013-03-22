@if "%1"=="" goto No_Arg1
@set DEV_FILE=%1
@goto End_Arg1
:No_Arg1
@set DEV_FILE=drfifo
:End_Arg1

@rem \src\vs2008\drfifo\Debug\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% write "Howdy, Pard."
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% write "Packet number 2."
@.\drfifoutil.exe %DEV_FILE% write "3rd packet."
@.\drfifoutil.exe %DEV_FILE% write "Test string."
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% read 40
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% read 16
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% read 12
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% read 14
@.\drfifoutil.exe %DEV_FILE% status
@.\drfifoutil.exe %DEV_FILE% read 100
@.\drfifoutil.exe %DEV_FILE% status
