#! /bin/bash

if [ -e SOURCES ]; then
    TARGETNAME=`grep ^TARGETNAME SOURCES | sed -e "s/^TARGETNAME *= *//"`
fi

if [ -z "$TARGETNAME" ]; then
    dir=`pwd`
    TARGETNAME=`basename "$dir"`
fi

if [ -z "$TARGETNAME" ]; then
    echo "Could not determine TARGETNAME."
    exit 1
fi

drivername="$TARGETNAME"
driver="${drivername}.sys"
DDK_DIR="C:\\WinDDK\\7600.16385.1"    # Change as needed.
DDK_BUILD_TYPE="fre"                  # Either "fre" or "chk".
obj64dir="obj${DDK_BUILD_TYPE}_win7_amd64/amd64/"
obj32dir="obj${DDK_BUILD_TYPE}_win7_x86/i386/"

obj64drv="$obj64dir${driver}"
obj32drv="$obj32dir${driver}"

rm -f "$obj64drv" "$obj32drv"

cmd.exe " /c build-x64.bat $DDK_DIR $DDK_BUILD_TYPE "
cmd.exe " /c build-x86.bat $DDK_DIR $DDK_BUILD_TYPE "

ls -l "$obj64drv" # "$obj32drv"
signtool.exe sign -v -f drSigningCert.pfx -p signme -t http://timestamp.verisign.com/scripts/timestamp.dll "$obj64drv"
signtool.exe sign -v -f drSigningCert.pfx -p signme -t http://timestamp.verisign.com/scripts/timestamp.dll "$obj32drv"
ls -l "$obj64drv" # "$obj32drv"
