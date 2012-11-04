/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2012 Neil Jerram
**
** Contact: neil@ossau.homelinux.net
**
** This file may be used under the terms of the GNU General Public License
** version 2.0 as published by the Free Software Foundation and appearing
** in the file LICENSE.GPL included in the packaging of this file.
**
** Please review the following information to ensure GNU General Public
** Licensing requirements will be met:
**     http://www.fsf.org/licensing/licenses/info/GPLv2.html.
**
**
****************************************************************************/

#include "oggcontentplugin.h"
#include <qmimetype.h>
#include <QtDebug>
#include <qtopialog.h>
#include <ivorbisfile.h>

/*!
    \class OggContentPlugin
    \internal

    Plugin for reading content information from OGG tags in media files.
*/

OggContentPlugin::OggContentPlugin()
{
    qLog(DocAPI) << "OggContentPlugin()";
}

OggContentPlugin::~OggContentPlugin()
{
    qLog(DocAPI) << "~OggContentPlugin()";
}

QStringList OggContentPlugin::keys() const
{
    qLog(DocAPI) << "OggContentPlugin::keys()";
    return QMimeType::fromId( QLatin1String( "audio/ogg" ) ).extensions();
}

bool OggContentPlugin::installContent( const QString &filePath, QContent *content )
{
    bool success = false;
    FILE *fp;
    OggVorbis_File ovfile;
    int rc;
    vorbis_comment *vc;
    int ii;

    fp = fopen(filePath.toUtf8().constData(), "r");

    if (fp != NULL) {

	qLog(DocAPI) << filePath;

	rc = ov_open(fp, &ovfile, NULL, 0);

	if (rc == 0) {

	    vc = ov_comment(&ovfile, -1);

	    if (vc != NULL) {

		for (ii = 0; ii < vc->comments; ii++) {

		    struct {
			const char *prefix;
			QContent::Property property;
		    } *mapping, mappings[] = {
			{ "ALBUM=", QContent::Album },
			{ "TITLE=", QContent::Title },
			{ "ARTIST=", QContent::Artist },
			{ "TRACKNUMBER=", QContent::Track },
			{ NULL, QContent::Album }
		    };

		    qLog(DocAPI) << QString::fromUtf8(vc->user_comments[ii],
						      vc->comment_lengths[ii]);

		    for (mapping = mappings; mapping->prefix; mapping++) {
			if (strstr(vc->user_comments[ii], mapping->prefix) ==
			    vc->user_comments[ii]) {
			    QString value = QString::fromUtf8(vc->user_comments[ii] +
							      strlen(mapping->prefix),
							      vc->comment_lengths[ii] -
							      strlen(mapping->prefix));

			    // Prepend 0 to single digit tracks, so
			    // that sorting the track properties as
			    // strings - which is what the Media
			    // Player does - becomes equivalent to
			    // sorting them as numbers.
                            if( (mapping->property == QContent::Track) &&
				(value.count() == 1) )
                                value.prepend( QLatin1String( "0" ) );

			    content->setProperty(mapping->property, value);
			    break;
			}
		    }
		}

		success = true;

	    } else {

		qLog(DocAPI) << "ov_comment returned NULL";
	    }

	    ov_clear(&ovfile);

	} else {

	    qLog(DocAPI) << "ov_open returned " << rc;
	    fclose(fp);
	    qLog(DocAPI) << "fclose OK";
	}

    } else {

	qLog(DocAPI) << "Failed to open: " << filePath;

    }

    return success;
}

bool OggContentPlugin::updateContent( QContent *content )
{
    return installContent( content->fileName(), content );
}

QTOPIA_EXPORT_PLUGIN(OggContentPlugin);
