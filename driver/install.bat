@rem My installer:
@rem \src\vs2008\drvinstall\Debug\drvinstall c:\src\vs2008\drfifo\driver\objfre_win7_amd64\amd64\drfifo.sys

sc create drfifo binpath= c:\tmp\drfifo.sys type= kernel
@rem Sleep:
@ping 1.1.1.1 -n 1 -w 1000 > nul
sc start drfifo
