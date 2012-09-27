/****************************************************************************
**
** This file is part of the Qt Extended Opensource Package.
**
** Copyright (C) 2009 Trolltech ASA.
**
** Contact: Qt Extended Information (info@qtextended.org)
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

#include "browser.h"

#include <QImageReader>
#include <QKeyEvent>
#include <QStyle>
#include <QThumbnail>
#include <QTimeString>
#include <QDebug>
#include <QApplication>

#include <Qtopia>

#include <qmailaddress.h>
#include <qmailmessage.h>
#include <qmailtimestamp.h>

#include <limits.h>

static QString replyColor1, replyColor2;

static int findReplyColors()
{
    QColor textColor(QApplication::palette().color(QPalette::Text));
    QColor reply1(textColor), reply2(textColor);

    int r, g, b;
    textColor.getRgb(&r, &g, &b);

    int range[3] = { 0 };
    if (((r + g + b) / 3) > 127) {
        // This is a light color - make the reply colors darker than this
        range[0] = 0 - r;
        range[1] = 0 - g;
        range[2] = 0 - b;
    } else {
        // This is a dark color - make the reply colors lighter than this
        range[0] = 255 - r;
        range[1] = 255 - g;
        range[2] = 255 - b;
    }

    reply1.setRgb(r + (range[0] * 1 / 2), g + (range[1] * 1 / 2), b + (range[2] * 1 / 2));
    replyColor1 = reply1.name();

    reply2.setRgb(r + (range[0] * 1 / 4), g + (range[1] * 1 / 4), b + (range[2] * 1 / 4));
    replyColor2 = reply2.name();

    return 0;
}

static int replyColorInit(findReplyColors());


Browser::Browser( QWidget *parent  )
    : QTextBrowser( parent ),
      replySplitter( &Browser::handleReplies )
{
    setFrameStyle( NoFrame );
    setFocusPolicy( Qt::StrongFocus );
}

Browser::~Browser()
{
}

void Browser::scrollBy(int dx, int dy)
{
    scrollContentsBy( dx, dy );
}

void Browser::setResource( const QUrl& name, QVariant var )
{
    resourceMap[name] = var;
}

void Browser::clearResources()
{
    resourceMap.clear();
    numbers.clear();
}

QVariant Browser::loadResource(int type, const QUrl& name)
{
    if (resourceMap.contains(name))
        return resourceMap[name];

    return QTextBrowser::loadResource(type, name);
}

QList<QString> Browser::embeddedNumbers() const
{
    QList<QString> result;
#ifndef QTOPIA_NO_DIAL_FUNCTION
    QSet<QString> appended;

    foreach (const QString& number, numbers) {
        // See if we have this number already, or a whitespace/punctuation-differing variant
        QString minimalNumber(number);
        minimalNumber.remove(QRegExp("[^xpw\\d]"));

        if (!appended.contains(minimalNumber)) {
            appended.insert(minimalNumber);
            result.append(number);
        }
    }
#endif

    return result;
}

void Browser::setTextResource(const QUrl& name, const QString& textData)
{
    setResource(name, QVariant(textData));
}

void Browser::setImageResource(const QUrl& name, const QByteArray& imageData)
{
    // Create a image from the data
    QDataStream imageStream(&const_cast<QByteArray&>(imageData), QIODevice::ReadOnly);
    QImageReader imageReader(imageStream.device());

    // Max size should be bounded by our display window, which will possibly
    // have a vertical scrollbar (and a small fudge factor...)
    int maxWidth = (width() - style()->pixelMetric(QStyle::PM_ScrollBarExtent) - 4);

    QSize imageSize;
    if (imageReader.supportsOption(QImageIOHandler::Size)) {
        imageSize = imageReader.size();

        // See if the image needs to be down-scaled during load
        if (imageSize.width() > maxWidth)
        {
            // And the loaded size should maintain the image aspect ratio
            imageSize.scale(maxWidth, (INT_MAX >> 4), Qt::KeepAspectRatio);
            imageReader.setQuality( 49 ); // Otherwise Qt smooth scales
            imageReader.setScaledSize(imageSize);
        }
    }

    QImage image = imageReader.read();

    if (!imageReader.supportsOption(QImageIOHandler::Size)) {
        // We need to scale it down now
        if (image.width() > maxWidth)
            image = image.scaled(maxWidth, INT_MAX, Qt::KeepAspectRatio);
    }

    setResource(name, QVariant(image));
}

void Browser::setPartResource(const QMailMessagePart& part)
{
    QString partId = Qt::escape(part.displayName());
    QUrl url(partId);

    if (!resourceMap.contains(url)) {
        if (part.contentType().type().toLower() == "text")
            setTextResource(url, part.body().data());
        else if (part.contentType().type().toLower() == "image")
            setImageResource(url, part.body().data(QMailMessageBody::Decoded));
    }
}

void Browser::setSource(const QUrl &name)
{
    Q_UNUSED(name)
// We deal with this ourselves.
//    QTextBrowser::setSource( name );
}

void Browser::setMessage(const QMailMessage& email, bool plainTextMode)
{
    if (plainTextMode) {
        // Complete MMS messages must be displayed in HTML
        if (email.messageType() == QMailMessage::Mms) {
            QString mmsType = email.headerFieldText("X-Mms-Message-Type");
            if (mmsType.contains("m-retrieve-conf") || mmsType.contains("m-send-req"))
                plainTextMode = false;
        }
    }

    // Maintain original linelengths if display width allows it
    if (email.messageType() == QMailMessage::Sms) {
        replySplitter = &Browser::smsBreakReplies;
    } else {
        uint lineCharLength;
        if ( fontInfo().pointSize() >= 10 ) {
            lineCharLength = width() / (fontInfo().pointSize() - 4 );
        } else {
            lineCharLength = width() / (fontInfo().pointSize() - 3 );
        }

        // Determine how to split lines in text
        if ( lineCharLength >= 78 )
            replySplitter = &Browser::noBreakReplies;
        else
            replySplitter = &Browser::handleReplies;
    }

    if (plainTextMode)
        displayPlainText(&email);
    else
        displayHtml(&email);
}

void Browser::displayPlainText(const QMailMessage* mail)
{
    QString bodyText;

    if ( (mail->status() & QMailMessage::Incoming) && !(mail->status() & QMailMessage::Downloaded) ) {
        if ( !(mail->status() & QMailMessage::Removed) ) {
            bodyText += "\n" + tr("Awaiting download") + "\n";
            bodyText += tr("Size of message") + ": " + describeMailSize(mail->size());
        }
    } else {
        if (mail->hasBody()) {
            bodyText += mail->body().data();
        }
        else {
            if ( mail->multipartType() == QMailMessagePartContainer::MultipartAlternative ) {
                const QMailMessagePart* bestPart = 0;

                // Find the best alternative for text rendering
                for ( uint i = 0; i < mail->partCount(); i++ ) {
                    const QMailMessagePart &part = mail->partAt( i );

                    // TODO: A good implementation would be able to extract the plain text parts
                    // from text/html and text/enriched...

                    if (part.contentType().type().toLower() == "text") {
                        if (part.contentType().subType().toLower() == "plain") {
                            // This is the best part for us
                            bestPart = &part;
                            break;
                        }
                        else if (part.contentType().subType().toLower() == "html") {
                            // This is the worst, but still acceptable, text part for us
                            if (bestPart == 0)
                                bestPart = &part;
                        }
                        else  {
                            // Some other text - better than html, probably
                            if ((bestPart != 0) && (bestPart->contentType().subType().toLower() == "html"))
                                bestPart = &part;
                        }
                    }
                }

                if (bestPart != 0)
                    bodyText += bestPart->body().data() + "\n";
                else
                    bodyText += "\n<" + tr("Message part is not displayable") + ">\n";
            }
            else if ( mail->multipartType() == QMailMessagePartContainer::MultipartRelated ) {
                const QMailMessagePart* startPart = &mail->partAt(0);

                // If not specified, the first part is the start
                QByteArray startCID = mail->contentType().parameter("start");
                if (!startCID.isEmpty()) {
                    for ( uint i = 1; i < mail->partCount(); i++ ) 
                        if (mail->partAt(i).contentID() == startCID) {
                            startPart = &mail->partAt(i);
                            break;
                        }
                }

                // Render the start part, if possible
                if (startPart->contentType().type().toLower() == "text")
                    bodyText += startPart->body().data() + "\n";
                else
                    bodyText += "\n<" + tr("Message part is not displayable") + ">\n";
            }
            else {
                // According to RFC 2046, any unrecognised type should be treated as 'mixed'
                if (mail->multipartType() != QMailMessagePartContainer::MultipartMixed)
                    qWarning() << "Unimplemented multipart type:" << mail->contentType().toString();

                // Render each succesive part to text, where possible
                for ( uint i = 0; i < mail->partCount(); i++ ) {
                    const QMailMessagePart &part = mail->partAt( i );

                    if (part.contentType().type().toLower() == "text") {
                        bodyText += part.body().data() + "\n";
                    } else {
                        bodyText += "\n<" + tr("Part") + ": " + part.displayName() + ">\n";
                    }
                }
            }
        }
    }

    QString text;

    if ((mail->messageType() != QMailMessage::Sms) && (mail->messageType() != QMailMessage::Instant))
        text += tr("Subject") + ": " + mail->subject() + "\n";

    QMailAddress fromAddress(mail->from());
    if (!fromAddress.isNull())
        text += tr("From") + ": " + fromAddress.toString() + "\n";

    if (mail->to().count() > 0) {
        text += tr("To") + ": ";
        text += QMailAddress::toStringList(mail->to()).join(", ");
    }
    if (mail->cc().count() > 0) {
        text += "\n" + tr("CC") + ": ";
        text += QMailAddress::toStringList(mail->cc()).join(", ");
    }
    if (mail->bcc().count() > 0) {
        text += "\n" + tr("BCC") + ": ";
        text += QMailAddress::toStringList(mail->bcc()).join(", ");
    }
    if ( !mail->replyTo().isNull() ) {
        text += "\n" + tr("Reply-To") + ": ";
        text += mail->replyTo().toString();
    }

    text += "\n" + tr("Date") + ": ";
    text += QTimeString::localYMDHMS( mail->date().toLocalTime(), QTimeString::Long ) + "\n";

    if (mail->status() & QMailMessage::Removed) {
        if (!bodyText.isEmpty()) {
            // Don't include the notice - the most likely reason to view plain text
            // is for printing, and we don't want to print the notice
        } else {
            text += "\n";
            text += tr("Message deleted from server");
        }
    }

    if (!bodyText.isEmpty()) {
        text += "\n";
        text += bodyText;
    }

    setPlainText(text);
}

static QString replaceLast(const QString container, const QString& before, const QString& after)
{
    QString result(container);

    int index;
    if ((index = container.lastIndexOf(before)) != -1)
        result.replace(index, before.length(), after);

    return result;
}

QString Browser::renderPart(const QMailMessagePart& part)
{
    QString result;

    QString partId = Qt::escape(part.displayName());

    QMailMessageContentType contentType = part.contentType();
    if ( contentType.type().toLower() == "text") { // No tr
        QString partText = part.body().data();
        if ( !partText.isEmpty() ) {
            if ( contentType.subType().toLower() == "html" ) {
                result = partText + "<br>";
            } else {
                result = formatText( partText );
            }
        }
    } else if ( contentType.type().toLower() == "image") { // No tr
        setPartResource(part);
        result = "<img src =\"" + partId + "\"></img>";
    } else {
        result = renderAttachment(part);
    }

    return result;
}

QString Browser::renderAttachment(const QMailMessagePart& part)
{
    QString partId = Qt::escape(part.displayName());

    QString attachmentTemplate = 
"<hr><b>ATTACHMENT_TEXT</b>: <font size=\"-1\"><a href=\"attachment;view;ATTACHMENT_NUMBER\">NAME_TEXT</a></font><br>";

    attachmentTemplate = replaceLast(attachmentTemplate, "ATTACHMENT_TEXT", tr("Attachment"));
    attachmentTemplate = replaceLast(attachmentTemplate, "ATTACHMENT_NUMBER", QString::number(part.partNumber()));
    return replaceLast(attachmentTemplate, "NAME_TEXT", partId);
}

typedef QPair<QString, QString> TextPair;

void Browser::renderMultipart(const QMailMessagePartContainer *mail, QString& bodyText)
{
    if ( mail->multipartType() == QMailMessagePartContainer::MultipartAlternative ) {

	int partIndex = -1;

	// Find the best alternative for rendering
	for ( uint i = 0; i < mail->partCount(); i++ ) {
	    const QMailMessagePart &part = mail->partAt( i );

	    // Parst are ordered simplest to most complex
	    if ((part.contentType().type().toLower() == "text") ||
		(part.contentType().type().toLower() == "image")) {
		// These parts are displayable
		partIndex = i;
	    }
	}

	if (partIndex != -1)
	    bodyText += renderPart(mail->partAt(partIndex));
	else
	    bodyText += "\n<" + tr("No displayable part") + ">\n";

    } else if ( mail->multipartType() == QMailMessagePartContainer::MultipartSigned) {
		
	// A multipart/signed message is expected to contain two
	// parts, a body part and a signature, and the signature
	// should be verified against the whole of the body part.  For
	// now, though, we don't do that verification; we just loop
	// through and render the parts - just as we do for
	// multipart/mixed, except without allowing for any further
	// multipart levels.
	for ( uint i = 0; i < mail->partCount(); i++) {
	    const QMailMessagePart &part = mail->partAt( i );

	    QMailMessageContentDisposition disposition = part.contentDisposition();
	    if (!disposition.isNull() && disposition.type() == QMailMessageContentDisposition::Attachment)
		bodyText += renderAttachment(part);
	    else
		bodyText += renderPart(part);
	}

    } else if ( mail->multipartType() == QMailMessagePartContainer::MultipartRelated ) {

	uint startIndex = 0;

	// If not specified, the first part is the start
	QByteArray startCID = mail->contentType().parameter("start");
	if (!startCID.isEmpty()) {
	    for ( uint i = 1; i < mail->partCount(); i++ ) 
		if (mail->partAt(i).contentID() == startCID) {
		    startIndex = i;
		    break;
		}
	}

	// Add any other parts as resources
	for ( uint i = 0; i < mail->partCount(); i++ ) {
	    if (i != startIndex)
		setPartResource(mail->partAt(i));
	}

	// Render the start part
	bodyText += renderPart(mail->partAt(startIndex));

    } else {

	// According to RFC 2046, any unrecognised type should be treated as 'mixed'
	if (mail->multipartType() != QMailMessagePartContainer::MultipartMixed)
	    qWarning() << "Unimplemented multipart type:" << mail->contentType().toString();

	// Render each part successively according to its disposition
	for ( uint i = 0; i < mail->partCount(); i++ ) {
	    const QMailMessagePart& part = mail->partAt(i);

	    if (part.partCount() > 0) {
		renderMultipart(&part, bodyText);
	    } else {
		QMailMessageContentDisposition disposition = part.contentDisposition();
		if (!disposition.isNull() && disposition.type() == QMailMessageContentDisposition::Attachment)
		    bodyText += renderAttachment(part);
		else
		    bodyText += renderPart(part);
	    }
	}
    }
}

void Browser::displayHtml(const QMailMessage* mail)
{
    QString subjectText, bodyText;
    QList<TextPair> metadata;

    // For SMS messages subject is the same as body, so for SMS don't 
    // show the message text twice (same for IMs)
    if ((mail->messageType() != QMailMessage::Sms) && (mail->messageType() != QMailMessage::Instant))
        subjectText = mail->subject();

    QString from = mail->headerFieldText("From");
    if (!from.isEmpty() && from != "\"\" <>") // ugh
        metadata.append(qMakePair(tr("From"), refMailTo( mail->from() )));

    if (mail->to().count() > 0) 
        metadata.append(qMakePair(tr("To"), listRefMailTo( mail->to() )));

    if (mail->cc().count() > 0) 
        metadata.append(qMakePair(tr("CC"), listRefMailTo( mail->cc() )));

    if (mail->bcc().count() > 0) 
        metadata.append(qMakePair(tr("BCC"), listRefMailTo( mail->bcc() )));

    if (!mail->replyTo().isNull())
        metadata.append(qMakePair(tr("Reply-To"), refMailTo( mail->replyTo() )));

    metadata.append(qMakePair(tr("Date"), QTimeString::localYMDHMS( mail->date().toLocalTime(), QTimeString::Long )));

    if ( (mail->status() & QMailMessage::Incoming) && !(mail->status() & QMailMessage::Downloaded) ) {
        if ( !(mail->status() & QMailMessage::Removed) ) {
            bodyText = 
    "<b>WAITING_TEXT</b><br>"
    "SIZE_TEXT<br>"
    "<br>"
    "<font size=\"-1\"><a href=\"download\">DOWNLOAD_TEXT</a></font>";

            bodyText = replaceLast(bodyText, "WAITING_TEXT", tr("Awaiting download"));
            bodyText = replaceLast(bodyText, "SIZE_TEXT", tr("Size of message") + ": " + describeMailSize(mail->size()));
            bodyText = replaceLast(bodyText, "DOWNLOAD_TEXT", tr("Download this message"));
        }
    } else {
        // See if this is a SMIL message
        bool isSmil = false;
#ifndef QTOPIA_NO_MMS
        if (mail->messageType() == QMailMessage::Mms) {
            QString mmsType = mail->headerFieldText("X-Mms-Message-Type");
            if (mmsType.contains("m-retrieve-conf") || mmsType.contains("m-send-req"))
                if (mail->contentType().content().toLower() == "multipart/related")
                    isSmil = true;
        }
#endif

        if (isSmil) {
            const QString smilTemplate = 
"<font size=\"-1\"><a href=\"attachment;play\"></b>PLAY_TEXT</b></a></font><hr><br>";
            
            bodyText = replaceLast(smilTemplate, "PLAY_TEXT", tr("Play MMS"));

            int showAtt = 0;
            for (uint i = 0; i < mail->partCount(); i++) {
                const QMailMessagePart &part = mail->partAt(i);
                if (part.contentType().type().toLower() == "text") {
                    QString subType = part.body().contentType().subType().toLower();
                    if (subType == "x-vcard" || subType == "x-vcalendar") {
                        QString headerTemplate = 
"<hr><br><b>ATTACHMENT_TEXT: </b><br>";
                        QString attachmentTemplate = 
"<font size=\"-1\"><a href=\"attachment;view\">NAME_TEXT</a></font>";

                        if (showAtt++ == 0)
                            bodyText.append(replaceLast(headerTemplate, "ATTACHMENT_TEXT", tr("Attachments")));
                        bodyText.append(replaceLast(attachmentTemplate, "NAME_TEXT", Qt::escape(part.displayName())));
                    }
                }
            }
        } else if (mail->partCount() > 0) {
	    renderMultipart(mail, bodyText);
        } else if (mail->messageType() == QMailMessage::System) {
            // Assume this is appropriately formatted
            bodyText = mail->body().data();
        } else {
#ifndef QTOPIA_NO_SMS
            if (mail->contentType().content().toLower() == "text/x-vcard") {
                QString body = mail->body().data();
                QList<QContact> contacts = QContact::readVCard(body.toLatin1());

                QString contactText;
                if ( contacts.count() == 0 ) {
                    // Invalid VCard data, so just show raw data
                    contactText = mail->body().data();
                } else if ( contacts.count() == 1 ) {
                    QString name = tr( "Message contains vCard describing %1" );
                    QContact& contact = contacts[0];
                    if ( !contact.nickname().isEmpty() ) {
                        contactText = name.arg( contact.nickname() );
                    } else if ( !contact.firstName().isEmpty() && !contact.lastName().isEmpty() ) {
                        contactText = name.arg( contact.firstName() + " " + contact.lastName() );
                    } else if ( !contact.firstName().isEmpty() ) {
                        contactText = name.arg( contact.firstName() );
                    } else if ( !contact.lastName().isEmpty() ) {
                        contactText = name.arg( contact.lastName() );
                    } else {
                        contactText = tr( "Message contains vCard describing a contact" );
                    }
                } else if ( contacts.count() > 1 ) {
                    contactText = tr( "Message contains vCard describing multiple contacts" );
                }
                if (!contactText.isEmpty()) {
                    bodyText = formatText( contactText );
                }
            } else 
#endif
            {
                bodyText = formatText( mail->body().data() );
            }
        }
    }

    // Form our parts into a displayable page
    QString pageData;

    if (!subjectText.isEmpty()) {
        QString subjectTemplate = 
#ifdef QTOPIA_HOMEUI
"<div align=left><b>Received</b>: RECEIVED_TEXT<br><b>Subject</b>: SUBJECT_TEXT</div><hr>";

        QDateTime timeStamp(mail->date().toLocalTime());
        QString timeStampText(timeStamp.toString("MMMM d"));
        if (timeStamp.date().daysTo(QDate::currentDate()) > 365)
            timeStampText += timeStamp.toString(" yyyy");
        timeStampText += timeStamp.toString(" h:mmap");

        subjectTemplate = replaceLast(subjectTemplate, "RECEIVED_TEXT", Qt::escape(timeStampText));
#else
"<div align=center><b><big><font color=LINK_COLOR>SUBJECT_TEXT</font></big></b></div><br>";

        QString linkColor = palette().link().color().name();
        subjectTemplate = replaceLast(subjectTemplate, "LINK_COLOR", QString("\"%1\"").arg(linkColor));
#endif

        pageData += replaceLast(subjectTemplate, "SUBJECT_TEXT", Qt::escape(subjectText));
    }

    if (mail->status() & QMailMessage::Removed) {
        QString noticeTemplate = 
"<div align=center>NOTICE_TEXT<br></div>";

        QString notice = tr("Message deleted from server");
        if (!bodyText.isEmpty()) {
            notice.prepend("<font size=\"-5\">[");
            notice.append("]</font>");
        }

        pageData += replaceLast(noticeTemplate, "NOTICE_TEXT", notice);
    }

    if (!bodyText.isEmpty()) {
        QString bodyTemplate = 
"<div align=left>BODY_TEXT</div>";

        pageData += replaceLast(bodyTemplate, "BODY_TEXT", bodyText);
    }

#ifndef QTOPIA_HOMEUI
    if (!metadata.isEmpty()) {
        QString metadataTemplate = 
"<hr><div align=left><font size=\"-5\">METADATA_TEXT</font></div>";

        QString itemTemplate = 
"<b>ID_TEXT: </b>CONTENT_TEXT<br>";

        QString metadataText;
        foreach (const TextPair item, metadata) {
            QString element = replaceLast(itemTemplate, "ID_TEXT", Qt::escape(item.first));
            element = replaceLast(element, "CONTENT_TEXT", item.second);
            metadataText.append(element);
        }

        pageData += replaceLast(metadataTemplate, "METADATA_TEXT", metadataText);
    }
#endif

    setHtml(pageData);
}

QString Browser::describeMailSize(uint bytes) const
{
    QString size;

    // No translation?
    if (bytes < 1024) {
        size.setNum(bytes);
        size += " Bytes";
    } else if (bytes < 1024*1024) {
        size.setNum( (bytes / 1024) );
        size += " KB";
    } else {
        float f = static_cast<float>( bytes )/ (1024*1024);
        size.setNum(f, 'g', 3);
        size += " MB";
    }

    return size;
}

QString Browser::formatText(const QString& txt) const
{
    return encodeUrlAndMail( (*this.*replySplitter)(txt) );
}

QString Browser::smsBreakReplies(const QString& txt) const
{
    /*  Preserve white space, add linebreaks so text is wrapped to
        fit the display width */
    QString str = "";
    QStringList p = txt.split("\n");

    QStringList::Iterator it = p.begin();
    while ( it != p.end() ) {
        str += buildParagraph( *it, "", true ) + "<br>";
        it++;
    }

    return str;
}

QString Browser::noBreakReplies(const QString& txt) const
{
    /*  Maintains the original linebreaks, but colours the lines
        according to reply level    */
    QString str = "";
    QStringList p = txt.split("\n");

    int x, levelList;
    QStringList::Iterator it = p.begin();
    while ( it != p.end() ) {

        x = 0;
        levelList = 0;
        while (x < (*it).length() ) {
            if ( (*it)[x] == '>' ) {
                levelList++;
            } else if ( (*it)[x] == ' ' ) {
            } else break;

            x++;
        }

        if (levelList == 0 ) {
            str += Qt::escape(*it) + "<br>";
        } else {
            const QString para("<font color=\"%1\">%2</font><br>");
            str += para.arg(levelList % 2 ? replyColor1 : replyColor2).arg(Qt::escape(*it));
        }

        it++;
    }

    return str;
}

QString appendLine(const QString& preceding, const QString& suffix)
{
    if (suffix.isEmpty())
        return preceding;

    QString result(preceding);

    int nwsIndex = QRegExp("[^\\s]").indexIn(suffix);
    if (nwsIndex > 0) {
        // This line starts with whitespace, which we'll have to protect to keep

        // We can't afford to make huge tracts of whitespace; ASCII art will be broken!
        // Convert any run of up to 4 spaces to a tab; convert all tabs to two spaces each
        QString leader(suffix.left(nwsIndex));
        leader.replace(QRegExp(" {1,4}"), "\t");

        // Convert the spaces to non-breaking
        leader.replace("\t", "&nbsp;&nbsp;");
        result.append(leader).append(suffix.mid(nwsIndex));
    }
    else
        result.append(suffix);

    return result;
}

QString unwrap(const QString& txt, const QString& prepend)
{
    QStringList lines = txt.split("\n", QString::KeepEmptyParts);

    QString result;
    result.reserve(txt.length());

    QStringList::iterator it = lines.begin(), prev = it, end = lines.end();
    if (it != end) {
        for (++it; it != end; ++prev, ++it) {
            QString terminator = "<br>";

            int prevLength = (*prev).length();
            if (prevLength == 0) {
                // If the very first line is empty, skip it; otherwise include
                if (prev == lines.begin())
                    continue;
            } else {
                int wsIndex = (*it).indexOf(QRegExp("\\s"));
                if (wsIndex == 0) {
                    // This was probably an intentional newline
                } else {
                    if (wsIndex == -1)
                        wsIndex = (*it).length();

                    bool logicalEnd = false;

                    const QChar last = (*prev)[prevLength - 1];
                    logicalEnd = ((last == '.') || (last == '!') || (last == '?'));

                    if ((*it)[0].isUpper() && logicalEnd) {
                        // This was probably an intentional newline
                    } else {
                        int totalLength = prevLength + prepend.length();
                        if ((wsIndex != -1) && ((totalLength + wsIndex) > 78)) {
                            // This was probably a forced newline - convert it to a space
                            terminator = " ";
                        }
                    }
                }
            }

            result = appendLine(result, Qt::escape(*prev) + terminator);
        }
        if (!(*prev).isEmpty())
            result = appendLine(result, Qt::escape(*prev));
    }

    return result;
}

/*  This one is a bit complicated.  It divides up all lines according
    to their reply level, defined as count of ">" before normal text
    It then strips them from the text, builds the formatted paragraph
    and inserts them back into the beginning of each line.  Probably not
    too speed efficient on large texts, but this manipulation greatly increases
    the readability (trust me, I'm using this program for my daily email reading..)
*/
QString Browser::handleReplies(const QString& txt) const
{
    QStringList out;
    QStringList p = txt.split("\n");
    QList<uint> levelList;
    QStringList::Iterator it = p.begin();
    uint lastLevel = 0, level = 0;

    // Skip the last string, if it's non-existent
    int offset = (txt.endsWith("\n") ? 1 : 0);

    QString str, line;
    while ( (it + offset) != p.end() ) {
        line = (*it);
        level = 0;

        if ( line.startsWith(">") ) {
            for (int x = 0; x < line.length(); x++) {
                if ( line[x] == ' ') {  
                    // do nothing
                } else if ( line[x] == '>' ) {
                    level++;
                    if ( (level > 1 ) && (line[x-1] != ' ') ) {
                        line.insert(x, ' ');    //we need it to be "> > " etc..
                        x++;
                    }
                } else {
                    // make sure it follows style "> > This is easier to format"
                    if ( line[x - 1] != ' ' )
                        line.insert(x, ' ');
                    break;
                }
            }
        }

        if ( level != lastLevel ) {
            if ( !str.isEmpty() ) {
                out.append( str );
                levelList.append( lastLevel );
            }

            str.clear();
            lastLevel = level;
            it--;
        } else {
            str += line.mid(level * 2) + "\n";
        }

        it++;
    }
    if ( !str.isEmpty() ) {
        out.append( str );
        levelList.append( level );
    }

    str = "";
    lastLevel = 0;
    int pos = 0;
    it = out.begin();
    while ( it != out.end() ) {
        if ( levelList[pos] == 0 ) {
            str += unwrap( *it, "" ) + "<br>";
        } else {
            QString pre = "";
            QString preString = "";
            for ( uint x = 0; x < levelList[pos]; x++) {
                pre += "&gt; ";
                preString += "> ";
            }

            QString segment = unwrap( *it, preString );

            const QString para("<font color=\"%1\">%2</font><br>");
            str += para.arg(levelList[pos] % 2 ? replyColor1 : replyColor2).arg(pre + segment);
        }

        lastLevel = levelList[pos];
        pos++;
        it++;
    }

    if ( str.endsWith("<br>") ) {
        str.chop(4);   //remove trailing br
    }

    return str;
}

QString Browser::buildParagraph(const QString& txt, const QString& prepend, bool preserveWs) const
{
    Q_UNUSED(prepend);
    QStringList out;

    //use escape here so we don't clutter our <br>
    QString input = Qt::escape( preserveWs ? txt : txt.simplified() );
    if (preserveWs)
        return input.replace("\n", "<br>");

    QStringList p = input.split( " ", QString::SkipEmptyParts );
    return p.join(" ");
}

static bool overlappingRange(int pos, int len, const QPair<int, int>& locator)
{
    // Two ways to avoid overlap:
    return !((locator.first >= (pos + len)) ||              // range starts after end
             ((locator.first + locator.second - 1) < pos)); // range ends before beginning
}

// Determine whether the supplied pattern overlaps the nominated range in the supplied string
static bool overlappingPattern(const QString& str, int pos, int len, QRegExp& pattern, QPair<int, int>& locator)
{
    // Have we exhausted instances of this pattern?
    if (locator.first == -1)
        return false;

    // Ensure it isn't already overlapping
    if (overlappingRange(pos, len, locator))
        return true;

    while ((locator.first != -1) && (locator.first < pos)) {
        // We need to search forward to find potential overlaps
        if ((locator.first = pattern.indexIn(str, locator.first + locator.second)) != -1) {
            locator.second = pattern.cap(0).length();

            if (overlappingRange(pos, len, locator))
                return true;
        }
    }

    return false;
}

/*  This one is called after Qt::escape, so if the email address is of type<some@rtg> we
    have to remove the safe characters at the beginning and end.  It's not a foolproof method, but
    it should handle nearly all cases.  To make it foolproof add methods to determine legal/illegal
    characters in the url/email addresses.
*/
QString Browser::encodeUrlAndMail(const QString& txt) const
{
    QString str(txt);

    // Find and encode email addresses
    QRegExp addressPattern(QMailAddress::emailAddressPattern());

    int pos = 0;
    while ((pos = addressPattern.indexIn(str, pos)) != -1) {
        QString capture = addressPattern.cap(0);

        // Ensure that encodings for < and > are not captured
        while (capture.startsWith("&lt;")) {
            capture = capture.mid(4);
            pos += 4;
        }
        while (capture.endsWith("&gt;")) {
            capture.chop(4);
        }

        QString ref(refMailTo(QMailAddress(capture)));
        str.replace(pos, capture.length(), ref);
        pos += ref.length();
    }

    // Find and encode http addresses
    const QString httpStr = "http://";
    const QString wwwStr = "www.";
    const QString validChars = QString(".!#$%'*+-/=?^_`{|}~");

    pos = 0;
    while ( ( ( str.indexOf(httpStr, pos) ) != -1 ) ||
            ( ( str.indexOf(wwwStr, pos) ) != -1 ) ) {
        int httpPos = str.indexOf(httpStr, pos);
        int wwwPos = str.indexOf(wwwStr, pos);
        int endPos = 0;
        QString urlPrefix;

        if ( (httpPos != -1) && ((wwwPos == -1) || (httpPos < wwwPos)) ) {
            pos = httpPos;
            endPos = pos + httpStr.length();
        } else {
            pos = wwwPos;
            endPos = pos + wwwStr.length();
            urlPrefix = "http://";
        }

        while ( endPos < static_cast<int>(str.length()) &&
                ( str[endPos].isLetterOrNumber() || (validChars.indexOf( str[endPos] ) != -1 )))
            endPos++;
        if (endPos >= static_cast<int>(str.length()) ||
            (!str[endPos].isLetterOrNumber() && (validChars.indexOf( str[endPos] ) == -1 )))
            endPos--;
        if (endPos < pos)
            endPos = pos; // Avoid infinite loop

        QString url = str.mid(pos, endPos - pos + 1);
        if ( url.indexOf('.') > -1 ) {  //Scan for . after // to verify that it is an url (weak, I know)
            QString s = "<font size=\"-1\"><a href=\"" + urlPrefix + url + "\">" + url + "</a></font>";
            str.replace(pos, endPos - pos + 1, s);

            pos += s.length();
        } else {
            pos = endPos + 1;
        }
    }

#ifndef QTOPIA_NO_DIAL_FUNCTION
    // Find and encode dialable numbers
    QRegExp numberPattern(QMailAddress::phoneNumberPattern());

    // Also, there are some patterns in the text we want to protect from
    // accidental number extraction: any number in an anchor should be left alone, 
    // and fonts, dates and times should be avoided
    QRegExp anchorPattern("< *a +href="                 // inital part
                          //".*?"                       // minimal match up to:
                          ".*"                          // no support for embedded minimal-matching?
                          "/(?: *a *)?>");              // terminator - either "/>" or "/a>"
    anchorPattern.setMinimal(true);

    QRegExp fontPattern("< *font"                       // inital part
                        //".*?"                         // minimal match up to:
                        ".*"                            // no support for embedded minimal-matching?
                        ">");                           // terminator; we only want to skip the declaration, not the content
    fontPattern.setMinimal(true);

    QRegExp datePattern("\\d{1,2}/"                     // one-or-two-digit day/month component, '/'
                        "\\d{1,2}"                      // one-or-two-digit day/month component
                        "(?:/\\d{2,4})?");              // optional: '/', two-or-four digit year component
    QRegExp timePattern("\\d{1,2}:"                     // one-or-two-digit hours component, ':'
                        "\\d{2,2}"                      // two-digit minutes component
                        "(?::\\d{2,2})?");              // optional: ':', two-digit seconds component

    QPair<int, int> anchorLocator(0, 0);
    QPair<int, int> fontLocator(0, 0);
    QPair<int, int> dateLocator(0, 0);
    QPair<int, int> timeLocator(0, 0);

    pos = 0;
    while ((pos = numberPattern.indexIn(str, pos)) != -1) {
        int len = numberPattern.cap(0).length();

        // Ensure we have a minimum of 6 characters
        QString number(numberPattern.cap(1));
        if (number.length() >= 6) {
            // We have found an acceptable pattern - now make sure it isn't part of
            // a date, time or anchor or font
            if (overlappingPattern(str, pos, len, anchorPattern, anchorLocator)) {
                pos = anchorLocator.first + anchorLocator.second;
            } else if (overlappingPattern(str, pos, len, fontPattern, fontLocator)) {
                pos = fontLocator.first + fontLocator.second;
            } else if (overlappingPattern(str, pos, len, datePattern, dateLocator)) {
                pos = dateLocator.first + dateLocator.second;
            } else if (overlappingPattern(str, pos, len, timePattern, timeLocator)) {
                pos = timeLocator.first + timeLocator.second;
            } else {
                // This pattern is not overlapped by any other - replace it with a link
                QString anchor(refNumber(number));
                str.replace(pos, len, anchor);
                pos += anchor.length();
                
                // The existing matches are now in new locations
                int increase(anchor.length() - len);
                if (anchorLocator.first != -1)
                    anchorLocator.first += increase;
                if (fontLocator.first != -1)
                    fontLocator.first += increase;
                if (dateLocator.first != -1)
                    dateLocator.first += increase;
                if (timeLocator.first != -1)
                    timeLocator.first += increase;

                // Add to the set of dialable numbers
                numbers.append(number);
            }
        } else {
            pos += len;
        }
    }
#endif

    return str;
}

QString Browser::listRefMailTo(const QList<QMailAddress>& list) const
{
    QStringList result;
    foreach ( const QMailAddress& address, list )
        result.append( refMailTo( address ) );

    return result.join( ", " );
}

QString Browser::refMailTo(const QMailAddress& address) const
{
    QString name = Qt::escape(address.displayName());
    if (name == "System")
        return name;

    if (address.isPhoneNumber() || address.isEmailAddress())
        return "<font size=\"-1\"><a href=\"mailto:" + Qt::escape(address.address()) + "\">" + name + "</a></font>";

    return name;
}

QString Browser::refNumber(const QString& number) const
{
    return "<font size=\"-1\"><a href=\"dial;" + Qt::escape(number) + "\">" + number + "</a></font>";
}

void Browser::keyPressEvent(QKeyEvent* event)
{
    const int factor = width() * 2 / 3;

    switch (event->key()) {
        case Qt::Key_Left:
            scrollBy(-factor, 0);
            event->accept();
            break;

        case Qt::Key_Right:
            scrollBy(factor, 0);
            event->accept();
            break;

        default:
            QTextBrowser::keyPressEvent(event);
            break;
    }
}

