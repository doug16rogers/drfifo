@rem My installer:
@rem \src\vs2008\drvinstall\Debug\drvinstall -u objfre_win7_amd64\amd64\drfifo.sys

sc stop drfifo
@rem Sleep:
@ping 1.1.1.1 -n 1 -w 1000 > nul
sc delete drfifo
