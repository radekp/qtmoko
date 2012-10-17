startup.files=startup/qpe.sh \
              startup/qpe.env
startup.path=/
startup.hint=script
INSTALLS+=startup

script.files=scripts/*
script.path=/bin
script.hint=script
INSTALLS+=script

f_dir.files=.directory
f_dir.path=/apps/Devtools
f_dir.trtarget=Devtools
f_dir.hint=desktop nct prep_db
INSTALLS+=f_dir
MODULES*=qtopia::prep_db

desktop.files+=desktop/*
desktop.path=/apps/Devtools
desktop.depends+=install_docapi_f_dir
desktop.hint=desktop
INSTALLS+=desktop

pics.files=*.png\
           *.svg
pics.path=/pics/devtools
pics.hint=pics
INSTALLS+=pics

startup.files=startup/qpe.sh \
              startup/qpe.env

startup.path=/
startup.hint=script
INSTALLS+=startup

help.source=help
help.files=qpe-devtools*
help.hint=help
INSTALLS+=help

