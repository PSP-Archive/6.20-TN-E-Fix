rm EBOOT.PBP

cd vshmenu
make clean
make
psp-packer vshmenu.prx
bin2c_64 vshmenu.prx ../systemctrl/vshmenu.h vshmenu
make clean

cd ../xmbctrl
make clean
make
#psp-packer xmbctrl.prx
bin2c xmbctrl.prx ../rebootex/xmbctrl.h xmbctrl
make clean

cd ../systemctrl
make clean
make
psp-packer-tn systemctrl.prx
bin2c systemctrl.prx ../rebootex/systemctrl.h systemctrl
make clean
rm vshmenu.h

cd ../rebootex
make clean
make
gzip -9 -n rebootex.bin
bin2c rebootex.bin.gz ../eboot/rebootex.h rebootex
make clean
rm rebootex.bin.gz
rm systemctrl.h
rm xmbctrl.h

cd ../eboot
make clean
make
move EBOOT.PBP ../EBOOT.PBP
make clean
rm rebootex.h

cd ..
copy EBOOT.PBP F:\PSP\GAME\TN_HEN\EBOOT.PBP