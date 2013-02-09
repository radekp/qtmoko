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
#ifndef QMAILMESSAGE_H
#define QMAILMESSAGE_H

#include "qmailaddress.h"
#include "qmailid.h"
#include "qmailtimestamp.h"
#include "qprivateimplementation.h"

#include <QByteArray>
#include <QFlags>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>


class QMailMessagePart;
class QMailMessagePartContainerPrivate;
class QDataStream;
class QTextStream;
class QFile;

class QMailMessageHeaderFieldPrivate;

class QTOPIAMAIL_EXPORT QMailMessageHeaderField : public QPrivatelyImplemented<QMailMessageHeaderFieldPrivate>
{
public:
    typedef QMailMessageHeaderFieldPrivate ImplementationType;

    typedef QPair<QByteArray, QByteArray> ParameterType;

    enum FieldType
    {
        StructuredField = 1,
        UnstructuredField = 2
    };

    QMailMessageHeaderField();
    QMailMessageHeaderField(const QByteArray& text, FieldType fieldType = StructuredField);
    QMailMessageHeaderField(const QByteArray& name, const QByteArray& text, FieldType fieldType = StructuredField);

    bool isNull() const;

    QByteArray id() const;
    void setId(const QByteArray& text);

    QByteArray content() const;
    void setContent(const QByteArray& text);

    QByteArray parameter(const QByteArray& name) const;
    void setParameter(const QByteArray& name, const QByteArray& value);

    bool isParameterEncoded(const QByteArray& name) const;
    void setParameterEncoded(const QByteArray& name);

    QList<ParameterType> parameters() const;

    virtual QByteArray toString(bool includeName = true, bool presentable = true) const;

    virtual QString decodedContent() const;

    bool operator== (const QMailMessageHeaderField& other) const;

    static QByteArray encodeWord(const QString& input, const QByteArray& charset = "");
    static QString decodeWord(const QByteArray& input);

    static QByteArray encodeParameter(const QString& input, const QByteArray& charset = "", const QByteArray& language = "");
    static QString decodeParameter(const QByteArray& input);

    static QByteArray encodeContent(const QString& input, const QByteArray& charset = "");
    static QString decodeContent(const QByteArray& input);

    static QByteArray removeComments(const QByteArray& input);
    static QByteArray removeWhitespace(const QByteArray& input);

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

protected:
    void parse(const QByteArray& text, FieldType fieldType);

private:
    friend class QMailMessageHeaderFieldPrivate;
    friend class QMailMessageHeaderPrivate;

    void output(QDataStream& out) const;
};

template <typename Stream> 
Stream& operator<<(Stream &stream, const QMailMessageHeaderField& field) { field.serialize(stream); return stream; }

template <typename Stream> 
Stream& operator>>(Stream &stream, QMailMessageHeaderField& field) { field.deserialize(stream); return stream; }


class QTOPIAMAIL_EXPORT QMailMessageContentType : public QMailMessageHeaderField
{
public:
    QMailMessageContentType();
    QMailMessageContentType(const QByteArray& type);
    QMailMessageContentType(const QMailMessageHeaderField& field);

    QByteArray type() const;
    void setType(const QByteArray& type);

    QByteArray subType() const;
    void setSubType(const QByteArray& subType);

    QByteArray name() const;
    void setName(const QByteArray& name);

    QByteArray boundary() const;
    void setBoundary(const QByteArray& boundary);

    QByteArray charset() const;
    void setCharset(const QByteArray& charset);

private:
    // Don't allow the Id to be changed
    void setId(const QByteArray& text);
};


class QTOPIAMAIL_EXPORT QMailMessageContentDisposition : public QMailMessageHeaderField
{
public:
    enum DispositionType
    {
        None = 0,
        Inline = 1,
        Attachment = 2
    };

    QMailMessageContentDisposition();
    QMailMessageContentDisposition(const QByteArray& type);
    QMailMessageContentDisposition(DispositionType disposition);
    QMailMessageContentDisposition(const QMailMessageHeaderField& field);

    DispositionType type() const;
    void setType(DispositionType disposition);

    QByteArray filename() const;
    void setFilename(const QByteArray& filename);

    QMailTimeStamp creationDate() const;
    void setCreationDate(const QMailTimeStamp& timeStamp);

    QMailTimeStamp modificationDate() const;
    void setModificationDate(const QMailTimeStamp& timeStamp);

    QMailTimeStamp readDate() const;
    void setReadDate(const QMailTimeStamp& timeStamp);

    int size() const;
    void setSize(int size);

private:
    // Don't allow the Id to be changed
    void setId(const QByteArray& text);
};


class QMailMessageHeaderPrivate;

// This class is not exposed to clients:
class QMailMessageHeader : public QPrivatelyImplemented<QMailMessageHeaderPrivate>
{
public:
    typedef QMailMessageHeaderPrivate ImplementationType;

    QMailMessageHeader();
    QMailMessageHeader(const QByteArray& input);

    void update(const QByteArray& id, const QByteArray& content);
    void append(const QByteArray& id, const QByteArray& content);
    void remove(const QByteArray& id);

    QMailMessageHeaderField field(const QByteArray& id) const;
    QList<QMailMessageHeaderField> fields(const QByteArray& id) const;

    QList<const QByteArray*> fieldList() const;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessageHeaderPrivate;
    friend class QMailMessagePartContainerPrivate;
    friend class QMailMessagePartPrivate;
    friend class QMailMessagePrivate;

    void output(QDataStream& out, const QList<QByteArray>& exclusions, bool stripInternal) const;
};

template <typename Stream> 
Stream& operator<<(Stream &stream, const QMailMessageHeader& header) { header.serialize(stream); return stream; }

template <typename Stream> 
Stream& operator>>(Stream &stream, QMailMessageHeader& header) { header.deserialize(stream); return stream; }


class QMailMessageBodyPrivate;
class LongString;

class QTOPIAMAIL_EXPORT QMailMessageBody : public QPrivatelyImplemented<QMailMessageBodyPrivate>
{
public:
    typedef QMailMessageBodyPrivate ImplementationType;

    enum TransferEncoding 
    {
        NoEncoding = 0,
        SevenBit = 1, 
        EightBit = 2, 
        Base64 = 3,
        QuotedPrintable = 4,
        Binary = 5, 
    };

    enum EncodingStatus
    {
        AlreadyEncoded = 1,
        RequiresEncoding = 2
    };

    enum EncodingFormat
    {
        Encoded = 1,
        Decoded = 2
    };

    // Construction functions
    static QMailMessageBody fromFile(const QString& filename, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status);

    static QMailMessageBody fromStream(QDataStream& in, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status);
    static QMailMessageBody fromData(const QByteArray& input, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status);

    static QMailMessageBody fromStream(QTextStream& in, const QMailMessageContentType& type, TransferEncoding encoding);
    static QMailMessageBody fromData(const QString& input, const QMailMessageContentType& type, TransferEncoding encoding);

    // Output functions
    bool toFile(const QString& filename, EncodingFormat format) const;

    QByteArray data(EncodingFormat format) const;
    bool toStream(QDataStream& out, EncodingFormat format) const;

    QString data() const;
    bool toStream(QTextStream& out) const;

    // Property accessors
    TransferEncoding transferEncoding() const;
    QMailMessageContentType contentType() const;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessagePartContainerPrivate;

    QMailMessageBody();

    uint indicativeSize() const;

    void output(QDataStream& out, bool includeAttachments) const;
    static QMailMessageBody fromLongString(LongString& ls, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status);
};

template <typename Stream> 
Stream& operator<<(Stream &stream, const QMailMessageBody& body) { body.serialize(stream); return stream; }

template <typename Stream> 
Stream& operator>>(Stream &stream, QMailMessageBody& body) { body.deserialize(stream); return stream; }

class QTOPIAMAIL_EXPORT QMailMessagePartContainer : public QPrivatelyImplemented<QMailMessagePartContainerPrivate>
{
public:
    typedef QMailMessagePartContainerPrivate ImplementationType;

    enum MultipartType 
    {
        MultipartNone = 0,
        MultipartSigned = 1,
        MultipartEncrypted = 2,
        MultipartMixed = 3,
        MultipartAlternative = 4,
        MultipartDigest = 5,
        MultipartParallel = 6,
        MultipartRelated = 7,
        MultipartFormData = 8,
        MultipartReport = 9
    };

    // Parts management interface:
    MultipartType multipartType() const;
    void setMultipartType(MultipartType type);

    uint partCount() const;
    void appendPart(const QMailMessagePart &part);
    void prependPart(const QMailMessagePart &part);

    const QMailMessagePart& partAt(uint pos) const;
    QMailMessagePart& partAt(uint pos);

    void clearParts();

    QByteArray boundary() const;
    void setBoundary(const QByteArray& text);

    // Body management interface:
    void setBody(const QMailMessageBody& body);
    QMailMessageBody body() const;

    bool hasBody() const;

    // Property accessors
    QMailMessageBody::TransferEncoding transferEncoding() const;
    QMailMessageContentType contentType() const;

    // Header fields describing this part container
    QString headerFieldText( const QString &id ) const;
    QMailMessageHeaderField headerField( const QString &id, QMailMessageHeaderField::FieldType fieldType = QMailMessageHeaderField::StructuredField ) const;

    QStringList headerFieldsText( const QString &id ) const;
    QList<QMailMessageHeaderField> headerFields( const QString &id, QMailMessageHeaderField::FieldType fieldType = QMailMessageHeaderField::StructuredField ) const;

    QList<QMailMessageHeaderField> headerFields() const;

    virtual void setHeaderField( const QString &id, const QString& content );
    virtual void setHeaderField( const QMailMessageHeaderField &field );

    virtual void appendHeaderField( const QString &id, const QString& content );
    virtual void appendHeaderField( const QMailMessageHeaderField &field );

    virtual void removeHeaderField( const QString &id );

protected:
    template<typename Subclass>
    QMailMessagePartContainer(Subclass* p);

    virtual void setHeader(const QMailMessageHeader& header, const QMailMessagePartContainerPrivate* parent = 0);

private:
    friend class QMailMessagePartContainerPrivate;

    uint indicativeSize() const;

    void outputParts(QDataStream& out, bool includePreamble, bool includeAttachments, bool stripInternal) const;
    void outputBody(QDataStream& out, bool includeAttachments) const;
};

class QMailMessagePartPrivate;

class QTOPIAMAIL_EXPORT QMailMessagePart : public QMailMessagePartContainer
{
public:
    typedef QMailMessagePartPrivate ImplementationType;

    QMailMessagePart();

    // Construction functions
    static QMailMessagePart fromFile(const QString& filename, const QMailMessageContentDisposition& disposition, 
                                     const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding, 
                                     QMailMessageBody::EncodingStatus status = QMailMessageBody::RequiresEncoding);

    static QMailMessagePart fromStream(QDataStream& in, const QMailMessageContentDisposition& disposition, 
                                       const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding, 
                                       QMailMessageBody::EncodingStatus status = QMailMessageBody::RequiresEncoding);
    static QMailMessagePart fromData(const QByteArray& input, const QMailMessageContentDisposition& disposition, 
                                     const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding, 
                                     QMailMessageBody::EncodingStatus status = QMailMessageBody::RequiresEncoding);

    static QMailMessagePart fromStream(QTextStream& in, const QMailMessageContentDisposition& disposition, 
                                       const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding);
    static QMailMessagePart fromData(const QString& input, const QMailMessageContentDisposition& disposition, 
                                     const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding);

    QString contentID() const;
    void setContentID(const QString &s);

    QString contentLocation() const;
    void setContentLocation(const QString &s);

    QString contentDescription() const;
    void setContentDescription(const QString &s);

    QMailMessageContentDisposition contentDisposition() const;
    void setContentDisposition(const QMailMessageContentDisposition& disposition);

    QString contentLanguage() const;
    void setContentLanguage(const QString &s);

    int partNumber() const;
    void setPartNumber(int);

    QString displayName() const;
    QString identifier() const;

    QString attachmentPath() const;
    bool detachAttachment(const QString &path);

    QString detachFileName() const;
    QString writeBodyTo(const QString &path) const;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessagePrivate;
    friend class QMailMessagePartContainerPrivate;

    void setAttachmentPath(const QString& path);

    void output(QDataStream& out, bool includeAttachments, bool stripInternal) const;
};

template <typename Stream> 
Stream& operator<<(Stream &stream, const QMailMessagePart& part) { part.serialize(stream); return stream; }

template <typename Stream> 
Stream& operator>>(Stream &stream, QMailMessagePart& part) { part.deserialize(stream); return stream; }

class QMailMessageMetaDataPrivate;

class QTOPIAMAIL_EXPORT QMailMessageMetaData : public QPrivatelyImplemented<QMailMessageMetaDataPrivate>
{
public:
    typedef QMailMessageMetaDataPrivate ImplementationType;

    enum MessageType
    {
        Mms     = 0x1,
        // was: Ems = 0x2
        Sms     = 0x4,
        Email   = 0x8,
        System  = 0x10,
        Instant = 0x20,
        None    = 0,
        AnyType = Mms | Sms | Email | System | Instant
    };

    static const quint64 &Incoming;
    static const quint64 &Outgoing;
    static const quint64 &Sent;
    static const quint64 &Replied;
    static const quint64 &RepliedAll;
    static const quint64 &Forwarded;
    static const quint64 &Downloaded;
    static const quint64 &Read;
    static const quint64 &Removed;
    static const quint64 &ReadElsewhere;
    static const quint64 &UnloadedData;
    static const quint64 &New;
    static const quint64 &All;

    enum ContentType {
        UnknownContent   = 0,
        NoContent        = 1,
        PlainTextContent = 2,
        RichTextContent  = 3,
        HtmlContent      = 4,
        ImageContent     = 5,
        AudioContent     = 6,
        VideoContent     = 7,
        MultipartContent = 8,
        SmilContent      = 9,
        VoicemailContent = 10,
        VideomailContent = 11,
        VCardContent     = 12,
        VCalendarContent = 13,
        ICalendarContent = 14,
        UserContent      = 64
    };

    QMailMessageMetaData();
    QMailMessageMetaData(const QMailMessageId& id);
    QMailMessageMetaData(const QString& uid, const QMailAccountId& accountId);

    virtual QMailMessageId id() const;
    virtual void setId(const QMailMessageId &id);

    virtual QMailFolderId parentFolderId() const;
    virtual void setParentFolderId(const QMailFolderId &id);

    virtual MessageType messageType() const;
    virtual void setMessageType(MessageType t);

    virtual QMailAddress from() const;
    virtual void setFrom(const QMailAddress &s);

    virtual QString subject() const;
    virtual void setSubject(const QString &s);

    virtual QMailTimeStamp date() const;
    virtual void setDate(const QMailTimeStamp &s);

    virtual QList<QMailAddress> to() const;
    virtual void setTo(const QList<QMailAddress>& s);
    virtual void setTo(const QMailAddress& s);

    virtual quint64 status() const;
    virtual void setStatus(quint64 newStatus);
    virtual void setStatus(quint64 mask, bool set);

    virtual QString fromAccount() const;
    virtual void setFromAccount(const QString &s);

    virtual QMailAccountId parentAccountId() const;
    virtual void setParentAccountId(const QMailAccountId& id);

    virtual QString fromMailbox() const;
    virtual void setFromMailbox(const QString &s);

    virtual QString serverUid() const;
    virtual void setServerUid(const QString &s);

    virtual uint size() const;
    virtual void setSize(uint i);

    virtual uint indicativeSize() const;

    virtual ContentType content() const;
    virtual void setContent(ContentType type);

    virtual QMailFolderId previousParentFolderId() const;
    virtual void setPreviousParentFolderId(const QMailFolderId &id);

    static quint64 statusMask(const QString &flagName);

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessage;
    friend class QMailMessagePrivate;
    friend class QMailStore;
    friend class QMailStorePrivate;

    virtual bool dataModified() const;
    virtual void committed();

    static void initStore();
};

class QMailMessagePrivate;

class QTOPIAMAIL_EXPORT QMailMessage : public QMailMessageMetaData, public QMailMessagePartContainer
{
public:
    using QMailMessageMetaData::MessageType;
    using QMailMessageMetaData::ContentType;

    // Mail content needs to use CRLF explicitly
    static const char CarriageReturn;
    static const char LineFeed;
    static const char* CRLF;

    enum AttachmentsAction {
        LinkToAttachments = 0,
        CopyAttachments,
        CopyAndDeleteAttachments
    };

    static QMailMessage fromRfc2822(const QByteArray &ba);
    static QMailMessage fromRfc2822File(const QString& fileName);

    QMailMessage();
    QMailMessage(const QMailMessageId& id);
    QMailMessage(const QString& uid, const QMailAccountId& accountId);

    enum EncodingFormat
    {
        HeaderOnlyFormat = 1,
        StorageFormat = 2,
        TransmissionFormat = 3,
        IdentityFormat = 4,
    }; 

    QByteArray toRfc2822(EncodingFormat format = TransmissionFormat) const;
    void toRfc2822(QDataStream& out, EncodingFormat format = TransmissionFormat) const;

    // Overrides of QMMPC functions where the data needs to be stored to the meta data also:

    virtual void setHeaderField( const QString &id, const QString& content );
    virtual void setHeaderField( const QMailMessageHeaderField &field );

    virtual void appendHeaderField( const QString &id, const QString& content );
    virtual void appendHeaderField( const QMailMessageHeaderField &field );

    virtual void removeHeaderField( const QString &id );

    // Overrides of QMMMD functions where the data needs to be stored to the part container also:

    virtual void setFrom(const QMailAddress &s);

    virtual void setSubject(const QString &s);
    
    virtual void setDate(const QMailTimeStamp &s);

    virtual void setTo(const QList<QMailAddress>& s);
    virtual void setTo(const QMailAddress& s);

    virtual uint indicativeSize() const;

    // Convenience functions:

    virtual QList<QMailAddress> cc() const;
    virtual void setCc(const QList<QMailAddress>& s);
    virtual QList<QMailAddress> bcc() const;
    virtual void setBcc(const QList<QMailAddress>& s);

    virtual QList<QMailAddress> recipients() const;
    virtual bool hasRecipients() const;

    virtual QMailAddress replyTo() const;
    virtual void setReplyTo(const QMailAddress &s);

    virtual QString inReplyTo() const;
    virtual void setInReplyTo(const QString &s);

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

protected:
    virtual void setHeader(const QMailMessageHeader& header, const QMailMessagePartContainerPrivate* parent = 0);

private:
    friend class QMailStore;
    friend class QMailStorePrivate;

    QMailMessageMetaDataPrivate* metaDataImpl();
    const QMailMessageMetaDataPrivate* metaDataImpl() const;

    QMailMessagePrivate* partContainerImpl();
    const QMailMessagePrivate* partContainerImpl() const;
    
    virtual bool contentModified() const;
    virtual void committed();

    QByteArray duplicatedData(const QString&) const;
    void updateMetaData(const QByteArray& id, const QString& value);

    static QMailMessage fromRfc2822(LongString& ls);
};

typedef QList<QMailMessage> QMailMessageList;
typedef QList<QMailMessageMetaData> QMailMessageMetaDataList;
typedef QList<QMailMessage::MessageType> QMailMessageTypeList;

Q_DECLARE_USER_METATYPE_ENUM(QMailMessageBody::TransferEncoding)
Q_DECLARE_USER_METATYPE_ENUM(QMailMessagePartContainer::MultipartType)
Q_DECLARE_USER_METATYPE_ENUM(QMailMessage::MessageType)
Q_DECLARE_USER_METATYPE_ENUM(QMailMessage::ContentType)
Q_DECLARE_USER_METATYPE_ENUM(QMailMessage::AttachmentsAction)

#ifndef SUPPRESS_REGISTER_QMAILMESSAGE_METATYPES
Q_DECLARE_USER_METATYPE(QMailMessage)
Q_DECLARE_USER_METATYPE(QMailMessageMetaData)

Q_DECLARE_METATYPE(QMailMessageList)
Q_DECLARE_METATYPE(QMailMessageMetaDataList)
Q_DECLARE_METATYPE(QMailMessageTypeList)
Q_DECLARE_USER_METATYPE_TYPEDEF(QMailMessageList, QMailMessageList)
Q_DECLARE_USER_METATYPE_TYPEDEF(QMailMessageMetaDataList, QMailMessageMetaDataList)
Q_DECLARE_USER_METATYPE_TYPEDEF(QMailMessageTypeList, QMailMessageTypeList)
#endif


// There is no good place to put this code; define it here, but we won't expose it to external clients
namespace QMail
{

    template<typename StringType>
    StringType unquoteString(const StringType& src)
    {
        // If a string has double-quote as the first and last characters, return the string
        // between those characters
        int length = src.length();
        if (length)
        {
            typename StringType::const_iterator const begin = src.constData();
            typename StringType::const_iterator const last = begin + length - 1;

            if ((last > begin) && (*begin == '"' && *last == '"'))
                return src.mid(1, length - 2);
        }

        return src;
    }

    template<typename StringType>
    StringType quoteString(const StringType& src)
    {
        StringType result("\"\"");

        // Return the input string surrounded by double-quotes, which are added if not present
        int length = src.length();
        if (length)
        {
            result.reserve(length + 2);

            typename StringType::const_iterator begin = src.constData();
            typename StringType::const_iterator last = begin + length - 1;

            if (*begin == '"')
                begin += 1;

            if ((last >= begin) && (*last == '"'))
                last -= 1;

            if (last >= begin)
                result.insert(1, StringType(begin, (last - begin + 1)));
        }

        return result;
    }

}

#endif
