/*
 *  Alessandro Briosi <alex@briosix.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 */
#define QTOPIA_BIN_PATH "/opt/Qtopia/bin/"

#include <QDebug>
#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDir>
#include <QAction>
#include <QProcess>
#include <QToolButton>


#ifdef QTOPIA_PHONE
#include <QSoftMenuBar>
#include <QContent>
#include <QStringList>
#else
#include <QMenuBar>
#endif

#include <QMenu>

#include "qurlview.h"
#include "qabstracturlmodel.h"
#include "qlocalurlmodel.h"
#include "qsfinputdialog.h"
#include "simplefm.h"


SimpleFM::SimpleFM(QWidget *parent, Qt::WindowFlags fl)
  : QMainWindow(parent,fl)
{
  setWindowTitle(tr("File Manager"));
  setCentralWidget(setupUI());
}

SimpleFM::~SimpleFM() 
{
}

QWidget *SimpleFM::setupUI() 
{
  QWidget *mainWidget = new QWidget();
  QToolButton *button = new QToolButton();
  cmbPath = new QComboBox(); 
  tbSessions = new QTabWidget(); 

  QVBoxLayout *vbox = new QVBoxLayout(mainWidget);
  QHBoxLayout *hbox = new QHBoxLayout();

  button->setFocusPolicy(Qt::NoFocus);
  lbPath = new QLabel();
  lbPath->setText(tr("Path") + ":");
  cmbPath->setEditable(true);
  cmbPath->addItem(QDir::homePath());
  cmbPath->setCurrentIndex(0);
  tbSessions->setFocusPolicy(Qt::StrongFocus);  
  currentView = 0;
  currentModel = 0;

  hbox->addWidget(lbPath,Qt::AlignLeft);

#ifdef QTOPIA_PHONE
  QMenu *m = QSoftMenuBar::menuFor(this);
#else
  lbPath->setSizePolicy(QSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred));
  hbox->addWidget(cmbPath,Qt::AlignHCenter);
  QMenu *m = menuBar()->addMenu(tr("Options"));
#endif

  hbox->addWidget(button,Qt::AlignRight);

  acFileOpen = m->addAction(tr("Open"));
  acPathOpen = m->addAction(tr("Open path")+" ...");
  acPathOpen->setEnabled(false);
  acNewDirectory = m->addAction(tr("New directory"));
  acFileRename = m->addAction(tr("Rename"));
  acSFPreferences = m->addAction(tr("Preferences"));
  acSFPreferences->setEnabled(false);
  m->addSeparator();
  QMenu *editM = new QMenu(tr("Edit"));
  acFileCopy = editM->addAction(tr("Copy"));
  acFileCut = editM->addAction(tr("Cut"));
  acFilePaste = editM->addAction(tr("Paste"));
  acFileDelete = editM->addAction(tr("Delete"));
  editM->addSeparator();
  acFileProperties = editM->addAction(tr("Properties")+" ...");
  acFileProperties->setEnabled(false);
  m->addMenu(editM);
  acParentDirectory = m->addAction(tr("Parent directory"));
  acParentDirectory->setEnabled(false);
  acFilePaste->setEnabled(false);
  acParentDirectory->setIcon(QIcon(":icon/up"));
  button->setDefaultAction(acParentDirectory);

  popupM = new QMenu(tr("Popup"));
  popupM->addAction(acFileOpen);
  popupM->addSeparator();
  popupM->addAction(acNewDirectory);
  popupM->addSeparator();
  popupM->addAction(acFileCopy);
  popupM->addAction(acFileCut);
  popupM->addAction(acFilePaste);
  popupM->addAction(acFileRename);
  popupM->addAction(acFileDelete);
  popupM->addSeparator();
  popupM->addAction(acFileProperties);

  connect (acFileOpen, SIGNAL(triggered()), this, SLOT(fileOpen()));
  connect (acPathOpen, SIGNAL(triggered()), this, SLOT(pathOpen()));
  connect (acNewDirectory, SIGNAL(triggered()), this, SLOT(newDirectory()));
  connect (acFileRename, SIGNAL(triggered()), this, SLOT(fileRename()));
  connect (acSFPreferences, SIGNAL(triggered()), this, SLOT(sfPreferences()));
  connect (acFileCopy, SIGNAL(triggered()), this, SLOT(fileCopy()));
  connect (acFileCut, SIGNAL(triggered()), this, SLOT(fileCut()));
  connect (acFilePaste, SIGNAL(triggered()), this, SLOT(filePaste()));
  connect (acFileDelete, SIGNAL(triggered()), this, SLOT(fileDelete()));
  connect (acFileProperties, SIGNAL(triggered()), this, SLOT(fileProperties()));
  connect (acParentDirectory, SIGNAL(triggered()), this, SLOT(parentDirectory()));

  addView(tr("Home"), "local",QDir::homePath(),QIcon(":icon/folder_home"));
  addView(tr("Root"), "local",QDir::rootPath(),QIcon(":icon/folder_red"));
  addView(tr("Root") + " <2>", "local",QDir::rootPath(),QIcon(":icon/folder_red"));
  connect(tbSessions, SIGNAL(currentChanged(int)),
                this, SLOT(currentViewChanged(int)));

  vbox->addLayout(hbox);
  vbox->addWidget(tbSessions);

  return mainWidget; 
}

void SimpleFM::addView(QString name, QString plugin, QString path, QIcon icon)
{
  QAbstractUrlModel *model;
  QUrlView *view;

  view = new QUrlView();

  /* we only have this right now */
  plugin = "local";

  if (plugin == "local") {
    model = new QLocalUrlModel(path);
  }
  view->setModel((QAbstractItemModel*)model);
  view->setPopUp(popupM);
  views.append(view);
  models.append(model);
  if (currentView == 0) {
    currentView = view;
    currentModel = model;
  }

#ifdef QTOPIA_PHONE
  lbPath->setText(currentModel->path());
#else
  connect(cmbPath, SIGNAL(activated(const QString)),
                    this, SLOT(cmbPathChanged(const QString)));
#endif
  connect(model, SIGNAL(pathChanged(QString)),
                this, SLOT(pathChanged(QString)));
  connect(view, SIGNAL(listItemActivated(int)),
                this, SLOT(listItemActivated(int)));
  tbSessions->addTab(view,icon,tr(name.toUtf8().constData()));
}

void SimpleFM::cmbPathChanged(const QString value)
{
  currentModel->setPath(value);
}

void SimpleFM::pathChanged(QString)
{
  acParentDirectory->setEnabled(!currentModel->isRoot());
#ifdef QTOPIA_PHONE
  lbPath->setText(currentModel->path());
#else
  cmbPath->clear();
  cmbPath->addItem(currentModel->path());
  cmbPath->setCurrentIndex(0);
#endif
}

void SimpleFM::listItemActivated(int)
{
  /* we could choose based on the model the default action to take ... */
  fileOpen(); 
}

void SimpleFM::fileOpen() 
{
  if (currentView->getSelected() >= 0) {
    if (currentModel->isDir(currentView->getSelected())) {
       currentModel->setPath(currentModel->fileName(currentView->getSelected()));
    } else if (currentModel->isExec(currentView->getSelected())) {
      /* run the file */
      QProcess::startDetached(currentModel->fullFileName(currentView->getSelected()));
    } else {
#ifdef QTOPIA_PHONE
      /* let's try opening the file with the mime app */
      QContent *doc = new QContent(currentModel->fullFileName(currentView->getSelected()),false);
      if (doc->executableName()!="") {
        QStringList sl;
        sl  << currentModel->fullFileName(currentView->getSelected());
qWarning()<<"trying to open the file";
        QProcess::startDetached(QTOPIA_BIN_PATH+doc->executableName(),sl);
      } else {
qWarning()<<"no executable associate with file type";
// shoud issue a warning???
      }
#endif
    }
  }
}

void SimpleFM::pathOpen()
{
  /* dialog for the path */
}

void SimpleFM::newDirectory()
{
  /* dialog for the name */
  bool ok;
  QString name = QSFInputDialog::getText(this, tr("New directory"),
                    tr("Create"), QLineEdit::Normal,
                    "",
                    &ok);
  if (ok && !name.isEmpty())
    currentModel->createDir(name);

}

void SimpleFM::sfPreferences()
{
  /* dialog for the preferences */
}

void SimpleFM::fileRename()
{
  bool ok;

  if (currentView->getSelected()<0) return;

  QString name = QSFInputDialog::getText(this, tr("Rename"),
                    tr("Rename in")+":", QLineEdit::Normal,
                    currentModel->fileName(currentView->getSelected()),
                    &ok);
  if (ok && !name.isEmpty()) {
    qWarning()<<"renaming "<<currentView->getSelected()<<name;
    currentModel->fileRename(currentView->getSelected(),name);
  }
}

void SimpleFM::fileCopy()
{
  /* memorize the path */
  if (currentView->getSelected() >= 0) {
    clipboard = currentModel->fullFileName(currentView->getSelected());
    clipboardaction = 0; /* copy */
  }
  acFilePaste->setEnabled(clipboard != "");
}

void SimpleFM::fileCut()
{
  if (currentView->getSelected() >= 0) {
    clipboard = currentModel->fullFileName(currentView->getSelected());
    clipboardaction = 1; /* move */
  } 
  acFilePaste->setEnabled(clipboard != "");
}

void SimpleFM::filePaste()
{
  if (clipboard != "")
    switch (clipboardaction) {
      case 0:
        currentModel->fileCopy(clipboard);
        clipboardaction=0;
/* should I do this???*/
        clipboard="";
        acFilePaste->setEnabled(false);
        break;
      case 1:
        currentModel->fileMove(clipboard);
        clipboardaction=0;
        clipboard="";
        acFilePaste->setEnabled(false);
        break;
    }
}

void SimpleFM::fileDelete()
{
  if (currentView->getSelected() >= 0)
    currentModel->fileDelete(currentModel->fullFileName(currentView->getSelected()));
}

void SimpleFM::fileProperties()
{
  if (currentView->getSelected() >= 0) {
    /* display file properties */
  }
}

void SimpleFM::parentDirectory()
{
  currentModel->setPath("..");
}

void SimpleFM::currentViewChanged(int index)
{
  currentView = views[index];
  currentModel = models[index];
  pathChanged(currentModel->path());  
}

