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

#include "qmailmessage.h"
#include "qprivateimplementationdef.h"

#include <qtopiaapplication.h> // needed for setenv
#include <qmimetype.h>
#include <qcontent.h>
#include <qmailaddress.h>
#include <qmailcodec.h>
#include <qmailstore.h>
#include <qmailtimestamp.h>
#include <qmailaccount.h>

#include "longstring_p.h"

#include <qfile.h>
#include <qfileinfo.h>
#include <stdlib.h>
#include <qtextstream.h>
#include <qtextcodec.h>
#include <qregexp.h>
#include <QTextCodec>
#include <QtDebug>
#include <ctype.h>
#include <limits.h>


static const QByteArray internalPrefix()
{
    static const QByteArray prefix("X-qtopia-internal-");
    return prefix;
}

template<typename CharType>
inline char toPlainChar(CharType value) { return value; }

template<>
inline char toPlainChar<QChar>(QChar value) { return static_cast<char>(value.unicode() & 0x7f); }

template<typename CharType>
inline bool asciiRepresentable(const CharType& value) { return ((value <= 127) && (value >= 0)); }

template<>
inline bool asciiRepresentable<unsigned char>(const unsigned char& value) { return (value <= 127); }

template<>
inline bool asciiRepresentable<signed char>(const signed char& value) { return (value >= 0); }

// The correct test for char depends on whether the platform defines char as signed or unsigned
// Default to signed char:
template<bool SignedChar>
inline bool asciiRepresentableChar(const char& value) { return asciiRepresentable(static_cast<signed char>(value)); }

template<>
inline bool asciiRepresentableChar<false>(const char& value) { return asciiRepresentable(static_cast<unsigned char>(value)); }

template<>
inline bool asciiRepresentable<char>(const char& value) { return asciiRepresentableChar<(SCHAR_MIN < CHAR_MIN)>(value); }

template<typename StringType>
QByteArray to7BitAscii(const StringType& src)
{
    QByteArray result;
    result.reserve(src.length());

    typename StringType::const_iterator it = src.begin();
    for (const typename StringType::const_iterator end = it + src.length(); it != end; ++it)
        if (asciiRepresentable(*it))
            result.append(toPlainChar(*it));

    return result;
}


// Parsing functions
static int insensitiveIndexOf(const QByteArray& content, const QByteArray& container, int from = 0)
{
    const char* const matchBegin = content.constData();
    const char* const matchEnd = matchBegin + content.length();

    const char* const begin = container.constData();
    const char* const end = begin + container.length() - (content.length() - 1);

    const char* it = begin + from;
    while (it < end)
    {
        if (toupper(*it++) == toupper(*matchBegin))
        {
            const char* restart = it;

            // See if the remainder matches
            const char* searchIt = it;
            const char* matchIt = matchBegin + 1;

            do 
            {
                if (matchIt == matchEnd)
                    return ((it - 1) - begin);

                // We may find the next place to search in our scan
                if ((restart == it) && (*searchIt == *(it - 1)))
                    restart = searchIt;
            }
            while (toupper(*searchIt++) == toupper(*matchIt++));

            // No match
            it = restart;
        }
    }

    return -1;
}

static bool insensitiveEqual(const QByteArray& lhs, const QByteArray& rhs)
{
    if (lhs.isNull() || rhs.isNull())
        return (lhs.isNull() && rhs.isNull());

    if (lhs.length() != rhs.length())
        return false;

    return insensitiveIndexOf(lhs, rhs) == 0;
}

static QByteArray charsetForInput(const QString& input)
{
    // See if this input needs encoding
    bool latin1 = false;

    const QChar* it = input.constData();
    const QChar* const end = it + input.length();
    for ( ; it != end; ++it)
    {
        if ((*it).unicode() > 0xff)
        {
            // Multi-byte characters included - we need to use UTF-8
            return QByteArray("UTF-8");
        }
        else if (!latin1 && ((*it).unicode() > 0x7f))
        {
            // We need encoding from latin-1
            latin1 = true;
        }
    }

    return (latin1? QByteArray("ISO-8859-1") : QByteArray());
}

static QTextCodec* codecForName(const QByteArray& charset, bool translateAscii = true)
{
    QByteArray encoding(charset.toLower());

    if (!encoding.isEmpty())
    {
        int index;

        if (translateAscii && encoding.contains("ascii")) 
        {
            // We'll assume the text is plain ASCII, to be extracted to Latin-1
            encoding = "ISO-8859-1";
        }
        else if ((index = encoding.indexOf('*')) != -1)
        {
            // This charset specification includes a trailing language specifier
            encoding = encoding.left(index);
        }

        return QTextCodec::codecForName(encoding);
    }

    return 0;
}

static QByteArray fromUnicode(const QString& input, const QByteArray& charset)
{
    if (!charset.isEmpty() && (insensitiveIndexOf("ascii", charset) == -1))
    {
        // See if we can convert using the nominated charset
        if (QTextCodec* textCodec = codecForName(charset))
            return textCodec->fromUnicode(input);

        qWarning() << "fromUnicode: unable to find codec for charset:" << charset;
    }

    return to7BitAscii(input.toLatin1());
}

static QString toUnicode(const QByteArray& input, const QByteArray& charset)
{
    if (!charset.isEmpty() && (insensitiveIndexOf("ascii", charset) == -1))
    {
        // See if we can convert using the nominated charset
        if (QTextCodec* textCodec = codecForName(charset))
            return textCodec->toUnicode(input);

        qWarning() << "toUnicode: unable to find codec for charset:" << charset;
    }

    return to7BitAscii(QString::fromLatin1(input.constData(), input.length()));
}

static QMailMessageBody::TransferEncoding encodingForName(const QByteArray& name)
{
    QByteArray ciName = name.toLower();

    if (ciName == "7bit")
        return QMailMessageBody::SevenBit;
    if (ciName == "8bit")
        return QMailMessageBody::EightBit;
    if (ciName == "base64")
        return QMailMessageBody::Base64;
    if (ciName == "quoted-printable")
        return QMailMessageBody::QuotedPrintable;
    if (ciName == "binary")
        return QMailMessageBody::Binary;

    return QMailMessageBody::NoEncoding;
}

static const char* nameForEncoding(QMailMessageBody::TransferEncoding te)
{
    switch( te ) 
    {
        case QMailMessageBody::SevenBit:
            return "7bit";
        case QMailMessageBody::EightBit:
            return "8bit";
        case QMailMessageBody::QuotedPrintable:
            return "quoted-printable";
        case QMailMessageBody::Base64:
            return "base64";
        case QMailMessageBody::Binary:
            return "binary";
        case QMailMessageBody::NoEncoding:
            break;
    }

    return 0;
}

static QMailCodec* codecForEncoding(QMailMessageBody::TransferEncoding te, bool textualData)
{
    switch( te ) 
    {
        case QMailMessageBody::NoEncoding:
        case QMailMessageBody::Binary:
            return new QMailPassThroughCodec();

        case QMailMessageBody::SevenBit:
        case QMailMessageBody::EightBit:
            return (textualData ? static_cast<QMailCodec*>(new QMailLineEndingCodec()) : new QMailPassThroughCodec());

        case QMailMessageBody::QuotedPrintable:
            return new QMailQuotedPrintableCodec(textualData ? QMailQuotedPrintableCodec::Text : QMailQuotedPrintableCodec::Binary, QMailQuotedPrintableCodec::Rfc2045);

        case QMailMessageBody::Base64:
            return new QMailBase64Codec(textualData ? QMailBase64Codec::Text : QMailBase64Codec::Binary);
    }

    return 0;
}

static QMailCodec* codecForEncoding(QMailMessageBody::TransferEncoding te, const QMailMessageContentType& content)
{
    return codecForEncoding(te, insensitiveEqual(content.type(), "text"));
}

//  Needs an encoded word of the form =?charset?q?word?=
static QString decodeWord(const QByteArray& encodedWord)
{
    QString result;
    int index[4];

    // Find the parts of the input
    index[0] = encodedWord.indexOf("=?");
    if (index[0] != -1)
    {
        index[1] = encodedWord.indexOf('?', index[0] + 2);
        if (index[1] != -1)
        {
            index[2] = encodedWord.indexOf('?', index[1] + 1);
            index[3] = encodedWord.lastIndexOf("?=");
            if ((index[2] != -1) && (index[3] > index[2]))
            {
                QByteArray charset = QMail::unquoteString(encodedWord.mid(index[0] + 2, (index[1] - index[0] - 2)));
                QByteArray encoding = encodedWord.mid(index[1] + 1, (index[2] - index[1] - 1)).toUpper();
                QByteArray encoded = encodedWord.mid(index[2] + 1, (index[3] - index[2] - 1));

                if (encoding == "Q")
                {
                    QMailQuotedPrintableCodec codec(QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2047);
                    result = codec.decode(encoded, charset);
                }
                else if (encoding == "B")
                {
                    QMailBase64Codec codec(QMailBase64Codec::Binary);
                    result = codec.decode(encoded, charset);
                }
            }
        }
    }

    if (result.isEmpty())
        result = encodedWord;

    return result;
}

static QByteArray generateEncodedWord(const QByteArray& codec, char encoding, const QByteArray& text)
{
    QByteArray result("=?");
    result.append(codec);
    result.append('?');
    result.append(encoding);
    result.append('?');
    result.append(text);
    result.append("?=");
    return result;
}

static QByteArray generateEncodedWord(const QByteArray& codec, char encoding, const QList<QByteArray>& list)
{
    QByteArray result;

    foreach (const QByteArray& item, list)
    {
        if (!result.isEmpty())
            result.append(' ');

        result.append(generateEncodedWord(codec, encoding, item));
    }

    return result;
}

static QList<QByteArray> split(const QByteArray& input, const QByteArray& separator)
{
    QList<QByteArray> result;

    int index = -1;
    int lastIndex = -1;
    do
    {
        lastIndex = index;
        index = input.indexOf(separator, lastIndex + 1);

        int offset = (lastIndex == -1 ? 0 : lastIndex + separator.length());
        int length = (index == -1 ? -1 : index - offset);
        result.append(input.mid(offset, length));
    } while (index != -1);

    return result;
}

static QByteArray encodeWord(const QString &text, const QByteArray& cs, bool* encoded)
{
    // Do we need to encode this input?
    QByteArray charset(cs);
    if (charset.isEmpty())
        charset = charsetForInput(text);

    if (encoded)
        *encoded = true;

    // We can't allow more than 75 chars per encoded-word, including the boiler plate...
    int maximumEncoded = 75 - 7 - charset.length();

    // If this is an encodedWord, we need to include any whitespace that we don't want to lose
    if (insensitiveIndexOf("utf-8", charset) == 0)
    {
        QMailBase64Codec codec(QMailBase64Codec::Binary, maximumEncoded);
        QByteArray encoded = codec.encode(text, charset);
        return generateEncodedWord(charset, 'B', split(encoded, QMailMessage::CRLF));
    }
    else if (insensitiveIndexOf("iso-8859-", charset) == 0)
    {
        QMailQuotedPrintableCodec codec(QMailQuotedPrintableCodec::Text, QMailQuotedPrintableCodec::Rfc2047, maximumEncoded);
        QByteArray encoded = codec.encode(text, charset);
        return generateEncodedWord(charset, 'Q', split(encoded, "=\n"));
    }

    if (encoded)
        *encoded = false;

    return to7BitAscii(text);
}

static QString decodeWordSequence(const QByteArray& str)
{
    static const QRegExp whitespace("^\\s+$");

    QString out;

    // Any idea why this isn't matching?
    //QRegExp encodedWord("\\b=\\?\\S+\\?\\S+\\?\\S*\\?=\\b");
    QRegExp encodedWord("=\\?\\S+\\?\\S+\\?\\S*\\?=");

    int pos = 0;
    int lastPos = 0;
    int length = str.length();

    while (pos != -1) {
        pos = encodedWord.indexIn(str, pos);
        if (pos != -1) {
            int endPos = pos + encodedWord.matchedLength();

            if ( ((pos == 0) || (::isspace(str[pos - 1]))) &&
                 ((endPos == length) || (::isspace(str[endPos]))) ) {

                QString preceding(str.mid(lastPos, (pos - lastPos)));
                QString decoded = decodeWord(str.mid(pos, (endPos - pos)));

                // If there is only whitespace between two encoded words, it should not be included
                if (!whitespace.exactMatch(preceding))
                    out.append(preceding);

                out.append(decoded);

                pos = endPos;
                lastPos = pos;
            }
            else
                pos = endPos;
        }
    }

    // Copy anything left
    out.append(str.mid(lastPos));

    return out;
}

enum TokenType
{
    Whitespace,
    Word,
    Quote
};

typedef QPair<const QChar*, int> TokenRange;
typedef QPair<TokenType, TokenRange> Token;

static Token makeToken(TokenType type, const QChar* begin, const QChar* end, bool escaped)
{
    return qMakePair(type, qMakePair(begin, (int)(end - begin) - (escaped ? 1 : 0)));
}

static QList<Token> tokenSequence(const QString& input)
{
    QList<Token> result;

    bool escaped = false;

    const QChar* it = input.constData();
    const QChar* const end = it + input.length();
    if (it != end) 
    {
        const QChar* token = it;
        TokenType state = ((*it) == '"' ? Quote : ((*it).isSpace() ? Whitespace : Word)); 

        for (++it; it != end; ++it) 
        {
            if (!escaped && (*it == '\\')) 
            {
                escaped = true;
                continue;
            }

            if (state == Quote)
            {
                // This quotation mark is a token by itself
                result.append(makeToken(state, token, it, escaped));

                state = ((*it) == '"' && !escaped ? Quote : ((*it).isSpace() ? Whitespace : Word)); 
                token = it;
            }
            else if (state == Whitespace)
            {
                if (!(*it).isSpace())
                {
                    // We have passed the end of this whitespace-sequence
                    result.append(makeToken(state, token, it, escaped));

                    state = ((*it) == '"' && !escaped ? Quote : Word);
                    token = it;
                }
            }
            else
            {
                if ((*it).isSpace() || ((*it) == '"' && !escaped))
                {
                    // We have passed the end of this word
                    result.append(makeToken(state, token, it, escaped));

                    state = ((*it).isSpace() ? Whitespace : Quote);
                    token = it;
                }
            }

            escaped = false;
        }

        result.append(makeToken(state, token, it, false));
    }

    return result;
}

static QByteArray encodeWordSequence(const QString& str, const QByteArray& charset)
{
    QByteArray result;

    bool quoted = false;
    bool tokenEncoded = false;
    QString quotedText;
    QString heldWhitespace;

    foreach (const Token& token, tokenSequence(str))
    {
        QString chars = QString::fromRawData(token.second.first, token.second.second);

        // See if we're processing some quoted words
        if (quoted)
        {
            if (token.first == Quote)
            {
                // We have reached the end of a quote sequence
                quotedText.append(chars);

                bool lastEncoded = tokenEncoded;

                QByteArray output = encodeWord(heldWhitespace + quotedText, charset, &tokenEncoded);

                quotedText = QString();
                quoted = false;
                heldWhitespace = QString();

                if (lastEncoded && tokenEncoded)
                    result.append(' ');
                result.append(output);
            }
            else
            {
                quotedText.append(chars);
            }
        }
        else
        {
            if (token.first == Quote)
            {
                // This token begins a quoted sequence
                quotedText = chars;
                quoted = true;
            }
            else
            {
                if (token.first == Word)
                {
                    bool lastEncoded = tokenEncoded;

                    // See if this token needs encoding
                    QByteArray output = encodeWord(heldWhitespace + chars, charset, &tokenEncoded);
                    heldWhitespace = QString();

                    if (lastEncoded && tokenEncoded)
                        result.append(' ');
                    result.append(output);
                }
                else // whitespace
                {
                    // If the last token was an encoded-word, we may need to include this
                    // whitespace into the next token
                    if (tokenEncoded)
                        heldWhitespace.append(chars);
                    else
                        result.append(chars);
                }
            }
        }
    }

    return result;
}

static int hexValue(char value)
{
    // Although RFC 2231 requires capitals, we may as well accept miniscules too
    if (value >= 'a')
        return (((value - 'a') + 10) & 0x0f);
    if (value >= 'A')
        return (((value - 'A') + 10) & 0x0f);

    return ((value - '0') & 0x0f);
}

static int hexValue(const char* it)
{
    return ((hexValue(*it) << 4) | hexValue(*(it + 1)));
}

static QString decodeParameterText(const QByteArray& text, const QByteArray& charset)
{
    QByteArray decoded;
    decoded.reserve(text.length());

    // Decode any encoded bytes in the data
    const char* it = text.constData();
    for (const char* const end = it + text.length(); it != end; ++it)
    {
        if (*it == '%')
        {
            if ((end - it) > 2)
                decoded.append(hexValue(it + 1));

            it += 2;
        }
        else
            decoded.append(*it);
    }

    // Decoded contains a bytestream - decode to unicode text if possible
    return toUnicode(decoded, charset);
}

//  Needs an encoded parameter of the form charset'language'text
static QString decodeParameter(const QByteArray& encodedParameter)
{
    QRegExp parameterFormat("([^']*)'(?:[^']*)'(.*)");
    if (parameterFormat.exactMatch(encodedParameter))
        return decodeParameterText(parameterFormat.cap(2).toLatin1(), parameterFormat.cap(1).toLatin1());

    // Treat the whole thing as input, and deafult the charset to ascii
    // This is not required by the RFC, since the input is illegal.  But, it 
    // seems ok since the parameter name has already indicated that the text 
    // should be encoded...
    return decodeParameterText(encodedParameter, "us-ascii");
}

static char hexRepresentation(int value)
{
    value &= 0x0f;

    if (value < 10)
        return ('0' + value);
    return ('A' + (value - 10));
}

static QByteArray generateEncodedParameter(const QByteArray& charset, const QByteArray& language, const QByteArray& text)
{
    QByteArray result(charset);
    QByteArray lang(language);

    // If the charset contains a language part, extract it
    int index = result.indexOf('*');
    if (index != -1)
    {
        // If no language is specfied, use the extracted part
        if (lang.isEmpty())
            lang = result.mid(index + 1);

        result = result.left(index);
    }

    result.append('\'');
    result.append(lang);
    result.append('\'');
    
    // Have a guess at how long the result will be
    result.reserve(result.length() + (2 * text.length()));

    // We could encode the exact set of permissible characters here, but they're basically the alphanumerics
    const char* it = text.constData();
    const char* const end = it + text.length();
    for ( ; it != end; ++it)
    {
        if (::isalnum(*it))
        {
            result.append(*it);
        }
        else
        {
            // Encode to hex
            int value = (*it);
            result.append('%').append(hexRepresentation(value >> 4)).append(hexRepresentation(value));
        }
    }

    return result;
}

static QByteArray encodeParameter(const QString &text, const QByteArray& charset, const QByteArray& language)
{
    QByteArray encoding(charset);
    if (encoding.isEmpty())
        encoding = charsetForInput(text);

    return generateEncodedParameter(encoding, language, fromUnicode(text, encoding));
}

static QByteArray removeComments(const QByteArray& input, int (*classifier)(int), bool acceptedResult = true)
{
    QByteArray result;

    int commentDepth = 0;
    bool quoted = false;
    bool escaped = false;

    const char* it = input.constData();
    const char* const end = it + input.length();
    for ( ; it != end; ++it ) {
        if ( !escaped && ( *it == '\\' ) ) {
            escaped = true;
            continue;
        }

        if ( *it == '(' && !escaped && !quoted ) {
            commentDepth += 1;
        }
        else if ( *it == ')' && !escaped && !quoted && ( commentDepth > 0 ) ) {
            commentDepth -= 1;
        }
        else {
            bool quoteProcessed = false;
            if ( !quoted && *it == '"' && !escaped ) {
                quoted = true;
                quoteProcessed = true;
            }

            if ( commentDepth == 0 ) {
                if ( quoted || (bool((*classifier)(*it)) == acceptedResult) )
                    result.append( *it );
            }

            if ( quoted && !quoteProcessed && *it == '"' && !escaped ) {
                quoted = false;
            }
        }

        escaped = false;
    }

    return result;
}

static QMailMessagePartContainer::MultipartType multipartTypeForName(const QByteArray& name)
{
    QByteArray ciName = name.toLower();

    if (ciName == "multipart/signed")
        return QMailMessagePartContainer::MultipartSigned;

    if (ciName == "multipart/encrypted")
        return QMailMessagePartContainer::MultipartEncrypted;

    if (ciName == "multipart/mixed")
        return QMailMessagePartContainer::MultipartMixed;

    if (ciName == "multipart/alternative")
        return QMailMessagePartContainer::MultipartAlternative;

    if (ciName == "multipart/digest")
        return QMailMessagePartContainer::MultipartDigest;

    if (ciName == "multipart/parallel")
        return QMailMessagePartContainer::MultipartParallel;

    if (ciName == "multipart/related")
        return QMailMessagePartContainer::MultipartRelated;

    if (ciName == "multipart/form")
        return QMailMessagePartContainer::MultipartFormData;

    if (ciName == "multipart/report")
        return QMailMessagePartContainer::MultipartReport;

    return QMailMessagePartContainer::MultipartNone;
}

static const char* nameForMultipartType(QMailMessagePartContainer::MultipartType type)
{
    switch( type ) 
    {
        case QMailMessagePartContainer::MultipartSigned:
        {
            return "multipart/signed";
        }
        case QMailMessagePartContainer::MultipartEncrypted:
        {
            return "multipart/encrypted";
        }
        case QMailMessagePartContainer::MultipartMixed:
        {
            return "multipart/mixed";
        }
        case QMailMessagePartContainer::MultipartAlternative:
        {
            return "multipart/alternative";
        }
        case QMailMessagePartContainer::MultipartDigest:
        {
            return "multipart/digest";
        }
        case QMailMessagePartContainer::MultipartParallel:
        {
            return "multipart/parallel";
        }
        case QMailMessagePartContainer::MultipartRelated:
        {
            return "multipart/related";
        }
        case QMailMessagePartContainer::MultipartFormData:
        {
            return "multipart/form-data";
        }
        case QMailMessagePartContainer::MultipartReport:
        {
            return "multipart/report";
        }
        case QMailMessagePartContainer::MultipartNone:
            break;
    }

    return 0;
}


// Necessary when writing to QDataStream, because the string/char literal is encoded
// in various pre-processed ways...

struct DataString
{
    DataString(char datum) : _datum(datum), _data(0), _length(0) {};
    DataString(const char* data) : _datum('\0'), _data(data), _length(strlen(_data)) {};
    DataString(const QByteArray& array) : _datum('\0'), _data(array.constData()), _length(array.length()) {};

    inline QDataStream& toDataStream(QDataStream& out) const
    {
        if (_data)
            out.writeRawData(_data, _length);
        else if (_datum == '\n')
            // Ensure that line-feeds are always CRLF sequences
            out.writeRawData(QMailMessage::CRLF, 2);
        else if (_datum != '\0')
            out.writeRawData(&_datum, 1);

        return out;
    }

private:
    char _datum;
    const char* _data;
    int _length;
};

QDataStream& operator<<(QDataStream& out, const DataString& dataString)
{
    return dataString.toDataStream(out);
}


/* QMailMessageHeaderField */
class QMailMessageHeaderFieldPrivate : public QPrivateImplementationBase
{
public:
    QMailMessageHeaderFieldPrivate();
    QMailMessageHeaderFieldPrivate(const QByteArray& text, bool structured);
    QMailMessageHeaderFieldPrivate(const QByteArray& name, const QByteArray& text, bool structured);

    bool operator== (const QMailMessageHeaderFieldPrivate& other) const;

    void addParameter(const QByteArray& name, const QByteArray& value);
    void parse(const QByteArray& text, bool structured);

    bool isNull() const;

    QByteArray id() const;
    void setId(const QByteArray& text);

    QByteArray content() const;
    void setContent(const QByteArray& text);

    QByteArray parameter(const QByteArray& name) const;
    void setParameter(const QByteArray& name, const QByteArray& value);

    bool isParameterEncoded(const QByteArray& name) const;
    void setParameterEncoded(const QByteArray& name);

    QList<QMailMessageHeaderField::ParameterType> parameters() const;

    QByteArray toString(bool includeName = true, bool presentable = true) const;

    void output(QDataStream& out) const;

    QString decodedContent() const;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

    QByteArray _id;
    QByteArray _content;
    bool _structured;
    QList<QMailMessageHeaderField::ParameterType> _parameters;
};

QMailMessageHeaderFieldPrivate::QMailMessageHeaderFieldPrivate()
    : QPrivateImplementationBase(this),
      _structured(true)
{
}

QMailMessageHeaderFieldPrivate::QMailMessageHeaderFieldPrivate(const QByteArray& text, bool structured)
    : QPrivateImplementationBase(this)
{
    parse(text, structured);
}

QMailMessageHeaderFieldPrivate::QMailMessageHeaderFieldPrivate(const QByteArray& id, const QByteArray& text, bool structured)
    : QPrivateImplementationBase(this)
{
    _id = id;
    parse(text, structured);
}

static bool validExtension(const QByteArray& trailer, int* number = 0, bool* encoded = 0)
{
    // Extensions according to RFC 2231:
    QRegExp extensionFormat("(?:\\*(\\d+))?(\\*?)");
    if (extensionFormat.exactMatch(trailer))
    {
        if (number)
            *number = extensionFormat.cap(1).toInt();
        if (encoded)
            *encoded = !extensionFormat.cap(2).isEmpty();

        return true;
    }
    else
        return false;
}

static bool matchingParameter(const QByteArray& name, const QByteArray& other, bool* encoded = 0)
{
    QByteArray match(name.trimmed());

    int index = insensitiveIndexOf(match, other);
    if (index == -1)
        return false;

    if (index > 0)
    {
        // Ensure that every preceding character is whitespace
        QByteArray leader(other.left(index).trimmed());
        if (!leader.isEmpty())
            return false;
    }

    int lastIndex = index + match.length() - 1;
    index = other.indexOf('=', lastIndex);
    if (index == -1)
        index = other.length();

    // Ensure that there is only whitespace between the matched name and the end of the name
    if ((index - lastIndex) > 1)
    {
        QByteArray trailer(other.mid(lastIndex + 1, (index - lastIndex)).trimmed());
        if (!trailer.isEmpty())
            return validExtension(trailer, 0, encoded);
    }

    return true;
}

void QMailMessageHeaderFieldPrivate::addParameter(const QByteArray& name, const QByteArray& value)
{
    _parameters.append(qMakePair(name, QMail::unquoteString(value)));
}

void QMailMessageHeaderFieldPrivate::parse(const QByteArray& text, bool structured)
{
    _structured = structured;

    // Parse text into main and params
    const char* const begin = text.constData();
    const char* const end = begin + text.length();

    bool malformed = false;

    const char* token = begin;
    const char* firstToken = begin;
    const char* it = begin;
    const char* separator = 0;
    for (bool quoted = false; it != end; ++it)
    {
        if (*it == '"') {
            quoted = !quoted;
        }
        else if (*it == ':' && !quoted && token == begin) {
            // This is the end of the field id
            if (_id.isEmpty()) {
                _id = QByteArray(token, (it - token)).trimmed();
                token = (it + 1);
            }
            else if (_structured) {
                // If this is a structured header, there can be only one colon
                token = (it + 1);
            }
            firstToken = token;
        }
        else if (*it == '=' && !quoted && structured) {
            if (separator == 0) {
                // This is a parameter separator
                separator = it;
            }
            else  {
                // It would be nice to identify extra '=' chars, but it's too hard
                // to separate them from encoded-word formations...
                //malformed = true;
            }
        }
        else if (*it == ';' && !quoted && structured) {
            // This is the end of a token
            if (_content.isEmpty()) {
                _content = QByteArray(token, (it - token)).trimmed();
            }
            else if ((separator > token) && ((separator + 1) < it)) {
                QByteArray name = QByteArray(token, (separator - token)).trimmed();
                QByteArray value = QByteArray(separator + 1, (it - separator - 1)).trimmed();

                if (!name.isEmpty() && !value.isEmpty())
                    addParameter(name, value);
            }
            else {
                malformed = true;
            }

            token = (it + 1);
            separator = 0;
        }
    }

    if (token != end) {
        if (_id.isEmpty()) {
            _id = QByteArray(token, (end - token)).trimmed();
        }
        else if (_content.isEmpty()) {
            _content = QByteArray(token, (end - token)).trimmed();
        }
        else if ((separator > token) && ((separator + 1) < end) && !malformed) {
            QByteArray name = QByteArray(token, (separator - token)).trimmed();
            QByteArray value = QByteArray(separator + 1, (end - separator - 1)).trimmed();

            if (!name.isEmpty() && !value.isEmpty())
                addParameter(name, value);
        }
        else if (_structured) {
            malformed = true;
        }
    }
}

bool QMailMessageHeaderFieldPrivate::operator== (const QMailMessageHeaderFieldPrivate& other) const
{
    if (!insensitiveEqual(_id, other._id))
        return false;
    
    if (_content != other._content)
        return false;

    if (_parameters.count() != other._parameters.count())
        return false;

    QList<QMailMessageHeaderField::ParameterType>::const_iterator it = _parameters.begin(), end = _parameters.end();
    QList<QMailMessageHeaderField::ParameterType>::const_iterator oit = other._parameters.begin();
    for ( ; it != end; ++it, ++oit)
        if (((*it).first != (*oit).first) || ((*it).second != (*oit).second))
            return false;

    return true;
}

bool QMailMessageHeaderFieldPrivate::isNull() const
{
    return (_id.isNull() && _content.isNull());
}

QByteArray QMailMessageHeaderFieldPrivate::id() const
{
    return _id;
}

void QMailMessageHeaderFieldPrivate::setId(const QByteArray& text)
{
    _id = text;
}

QByteArray QMailMessageHeaderFieldPrivate::content() const
{
    return _content;
}

void QMailMessageHeaderFieldPrivate::setContent(const QByteArray& text)
{
    _content = text;
}

QByteArray QMailMessageHeaderFieldPrivate::parameter(const QByteArray& name) const
{
    // Coalesce folded parameters into a single return value
    QByteArray result;

    QByteArray param = name.trimmed();
    foreach (const QMailMessageContentType::ParameterType& parameter, _parameters) {
        if (matchingParameter(param, parameter.first))
            result.append(parameter.second);
    }

    return result;
}

void QMailMessageHeaderFieldPrivate::setParameter(const QByteArray& name, const QByteArray& value)
{
    if (!_structured)
        return;

    QByteArray param = name.trimmed();

    bool encoded = false;
    int index = param.indexOf('*');
    if (index != -1) {
        encoded = true;
        param = param.left(index);
    }

    // Find all existing parts of this parameter, if present
    QList<QList<QMailMessageHeaderField::ParameterType>::iterator> matches;
    QList<QMailMessageHeaderField::ParameterType>::iterator it = _parameters.begin(), end = _parameters.end();
    for ( ; it != end; ++it) {
        if (matchingParameter(param, (*it).first))
            matches.prepend(it);
    }

    while (matches.count() > 1)
        _parameters.erase(matches.takeFirst());
    if (matches.count() == 1)
        it = matches.takeFirst();
    
    // If the value is too long to fit on one line, break it into manageable pieces
    const int maxInputLength = 78 - 9 - param.length();

    if (value.length() > maxInputLength) {
        // We have multiple pieces to insert
        QList<QByteArray> pieces;
        QByteArray input(value);
        do
        {
            pieces.append(input.left(maxInputLength));
            input = input.mid(maxInputLength);
        } while (input.length());

        if (it == end) {
            // Append each piece at the end
            int n = 0;
            while (pieces.count() > 0) {
                QByteArray id(param);
                id.append('*').append(QByteArray::number(n));
                if (encoded && (n == 0))
                    id.append('*');

                _parameters.append(qMakePair(id, pieces.takeFirst()));
                ++n;
            }
        }
        else {
            // Overwrite the remaining instance of the parameter, and place any 
            // following pieces immediately after
            int n = pieces.count() - 1;
            int initial = n;

            while (pieces.count() > 0) {
                QByteArray id(param);
                id.append('*').append(QByteArray::number(n));
                if (encoded && (n == 0))
                    id.append('*');

                QMailMessageHeaderField::ParameterType parameter = qMakePair(id, pieces.takeLast());
                if (n == initial) {
                    // Put the last piece into the existing position
                    (*it) = parameter;
                }
                else {
                    // Insert before the previous piece, and record the new iterator
                    it = _parameters.insert(it, parameter);
                }

                --n;
            }
        }
    }
    else {
        // Just one part to insert
        QByteArray id(param);
        if (encoded)
            id.append('*');
        QMailMessageHeaderField::ParameterType parameter = qMakePair(id, value);

        if (it == end) {
            _parameters.append(parameter);
        }
        else {
            (*it) = parameter;
        }
    }
}

bool QMailMessageHeaderFieldPrivate::isParameterEncoded(const QByteArray& name) const
{
    QByteArray param = name.trimmed();

    bool encoded = false;
    foreach (const QMailMessageContentType::ParameterType& parameter, _parameters)
        if (matchingParameter(param, parameter.first, &encoded))
            return encoded;

    return false;
}

void QMailMessageHeaderFieldPrivate::setParameterEncoded(const QByteArray& name)
{
    QByteArray param = name.trimmed();

    QList<QMailMessageHeaderField::ParameterType>::iterator it = _parameters.begin(), end = _parameters.end();
    for ( ; it != end; ++it) {
        bool encoded = false;
        if (matchingParameter(param, (*it).first, &encoded)) {
            if (!encoded)
                (*it).first.append('*');
        }
    }
}

static QByteArray protectedParameter(const QByteArray& value)
{
    static const QRegExp whitespace("\\s+");
    static const QRegExp tspecials = QRegExp("[<>\\[\\]\\(\\)\\?:;@\\\\,=]");

    if ((whitespace.indexIn(value) != -1) ||
        (tspecials.indexIn(value) != -1))
        return QMail::quoteString(value);
    else
        return value;
}

static bool extendedParameter(const QByteArray& name, QByteArray* truncated = 0, int* number = 0, bool* encoded = 0)
{
    QByteArray param(name.trimmed());

    int index = param.indexOf('*');
    if (index == -1)
        return false;

    if (truncated)
        *truncated = param.left(index).trimmed();

    return validExtension(param.mid(index), number, encoded);
}

QList<QMailMessageHeaderField::ParameterType> QMailMessageHeaderFieldPrivate::parameters() const
{
    QList<QMailMessageHeaderField::ParameterType> result;

    foreach (const QMailMessageContentType::ParameterType& param, _parameters) {
        QByteArray id;
        int number;
        if (extendedParameter(param.first, &id, &number)) {
            if (number == 0) {
                result.append(qMakePair(id, parameter(id)));
            }
        }
        else {
            result.append(param);
        }
    }

    return result;
}

QByteArray QMailMessageHeaderFieldPrivate::toString(bool includeName, bool presentable) const
{
    if (_id.isEmpty())
        return QByteArray();

    QByteArray result;
    if (includeName) {
        result = _id + ":";
    }
    
    if (!_content.isEmpty()) {
        if (includeName)
            result += ' ';
        result += _content;
    }

    if (_structured)
    {
        foreach (const QMailMessageContentType::ParameterType& parameter, (presentable ? parameters() : _parameters))
            result.append("; ").append(parameter.first).append('=').append(protectedParameter(parameter.second));
    }

    return result;
}

static void outputHeaderPart(QDataStream& out, const QByteArray& text, int* lineLength, const int maxLineLength)
{
    static const QRegExp whitespace("\\s");

    int remaining = maxLineLength - *lineLength;
    if (text.length() <= remaining)
    {
        out << DataString(text);
        *lineLength += text.length();
    }
    else
    {
        // See if we can find suitable whitespace to break the line
        int wsIndex = -1;
        int lastIndex = -1;
        int preferredIndex = -1;
        do 
        {
            lastIndex = wsIndex;
            if ((lastIndex > 0) && (text[lastIndex - 1] == ';')) {
                // Prefer to split after (possible) parameters
                preferredIndex = lastIndex;
            }

            wsIndex = whitespace.indexIn(text, wsIndex + 1);
        } while ((wsIndex != -1) && (wsIndex < remaining));

        if (preferredIndex != -1)
            lastIndex = preferredIndex;

        if (lastIndex == -1)
        {
            // We couldn't find any suitable whitespace - just break at the last char
            lastIndex = remaining;
        }

        if (lastIndex == 0)
        {
            out << DataString('\n') << DataString(text[0]);
            *lineLength = 1;
            lastIndex = 1;
        }
        else
        {
            out << DataString(text.left(lastIndex)) << DataString('\n');

            if (lastIndex == remaining) {
                // We need to insert some artifical whitespace
                out << DataString('\t');
            } else {
                // Append the breaking whitespace (ensure it does not get CRLF-ified)
                out << DataString(QByteArray(1, text[lastIndex]));
                ++lastIndex;
            }

            *lineLength = 1;
        }

        QByteArray remainder(text.mid(lastIndex));
        if (!remainder.isEmpty())
            outputHeaderPart(out, remainder, lineLength, maxLineLength);
    }
}

void QMailMessageHeaderFieldPrivate::output(QDataStream& out) const
{
    static const int maxLineLength = 78;

    if (_id.isEmpty())
        return;

    if (_structured) {
        qWarning() << "Unable to output structured header field:" << _id;
        return;
    }

    QByteArray element(_id);
    element.append(':');
    out << DataString(element);

    if (!_content.isEmpty()) {
        int lineLength = element.length();
        outputHeaderPart(out, " " + _content, &lineLength, maxLineLength);
    }

    out << DataString('\n');
}

static bool parameterEncoded(const QByteArray& name)
{
    QByteArray param(name.trimmed());
    if (param.isEmpty())
        return false;

    return (param[param.length() - 1] == '*');
}

QString QMailMessageHeaderFieldPrivate::decodedContent() const
{
    QString result(QMailMessageHeaderField::decodeContent(_content));

    if (_structured)
    {
        foreach (const QMailMessageContentType::ParameterType& parameter, _parameters) {
            QString decoded;
            if (parameterEncoded(parameter.first))
                decoded = QMailMessageHeaderField::decodeParameter(protectedParameter(parameter.second));
            else
                decoded = protectedParameter(parameter.second);
            result.append("; ").append(parameter.first).append('=').append(decoded);
        }
    }

    return result;
}

template <typename Stream> 
void QMailMessageHeaderFieldPrivate::serialize(Stream &stream) const
{
    stream << _id;
    stream << _content;
    stream << _structured;
    stream << _parameters;
}

template <typename Stream> 
void QMailMessageHeaderFieldPrivate::deserialize(Stream &stream)
{
    stream >> _id;
    stream >> _content;
    stream >> _structured;
    stream >> _parameters;
}

template class QPrivatelyImplemented<QMailMessageHeaderFieldPrivate>;


/*!
    \class QMailMessageHeaderField
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessageHeaderField class encapsulates the parsing of message header fields.
    
    \ingroup messaginglibrary
   
    QMailMessageHeaderField provides simplified access to the various components of the 
    header field, and allows the field content to be extracted in a standardized form.

    The content of a header field may be formed of unstructured text, or it may have an 
    internal structure.  If a structured field is specified, QMailMessageHeaderField assumes 
    that the contained header field is structured in a format equivalent to that used for the 
    RFC 2045 'Content-Type' and RFC 2183 'Content-Disposition' header fields.  If the field 
    is unstructured, or conforms to a different structure, then the parameter() and parameters() functions
    will return empty results, and the setParameter() function will have no effect.

    QMailMessageHeaderField contains static functions to assist in creating correct
    header field content, and presenting header field content.  The encodeWord() and 
    decodeWord() functions translate between plain text and the encoded-word specification
    defined in RFC 2045.  The encodeParameter() and decodeParameter() functions translate
    between plain text and the encoded-parameter format defined in RFC 2231.

    The removeWhitespace() function can be used to remove irrelevant whitespace characters
    from a string, and the removeComments() function can remove any comment sequences 
    present, encododed according to the RFC 2822 specification.
*/

/*!
    \typedef QMailMessageHeaderField::ImplementationType
    \internal
*/

/*!
    \typedef QMailMessageHeaderField::ParameterType
    \internal
*/

/*!
    \enum QMailMessageHeaderField::FieldType
    
    This enum type is used to describe the formatting of field content.

    \value StructuredField      The field content should be parsed assuming it is structured according to the specification for RFC 2045 'Content-Type' fields.
    \value UnstructuredField    The field content has no internal structure.
*/

/*!
    Creates an uninitialised message header field object.
*/
QMailMessageHeaderField::QMailMessageHeaderField()
    : QPrivatelyImplemented<QMailMessageHeaderFieldPrivate>(new QMailMessageHeaderFieldPrivate())
{
}

/*!
    Creates a message header field object from the data in \a text. If \a fieldType is 
    QMailMessageHeaderField::StructuredField, then \a text will be parsed assuming a 
    format equivalent to that used for the RFC 2045 'Content-Type' and 
    RFC 2183 'Content-Disposition' header fields.
*/
QMailMessageHeaderField::QMailMessageHeaderField(const QByteArray& text, FieldType fieldType)
    : QPrivatelyImplemented<QMailMessageHeaderFieldPrivate>(new QMailMessageHeaderFieldPrivate(text, (fieldType == StructuredField)))
{
}

/*!
    Creates a message header field object with the field id \a id and the content 
    data in \a text.  If \a fieldType is QMailMessageHeaderField::StructuredField, 
    then \a text will be parsed assuming a format equivalent to that used for the 
    RFC 2045 'Content-Type' and RFC 2183 'Content-Disposition' header fields.
*/
QMailMessageHeaderField::QMailMessageHeaderField(const QByteArray& id, const QByteArray& text, FieldType fieldType)
    : QPrivatelyImplemented<QMailMessageHeaderFieldPrivate>(new QMailMessageHeaderFieldPrivate(id, text, (fieldType == StructuredField)))
{
}

/*! \internal */
bool QMailMessageHeaderField::operator== (const QMailMessageHeaderField& other) const
{
    return impl(this)->operator==(*other.impl(&other));
}

/*!
    Returns true if the header field has not been initialized.
*/
bool QMailMessageHeaderField::isNull() const
{
    return impl(this)->isNull();
}

/*!
    Returns the ID of the header field.
*/
QByteArray QMailMessageHeaderField::id() const
{
    return impl(this)->id();
}

/*!
    Sets the ID of the header field to \a id.
*/
void QMailMessageHeaderField::setId(const QByteArray& id)
{
    impl(this)->setId(id);
}

/*!
    Returns the content of the header field, without any associated parameters.
*/
QByteArray QMailMessageHeaderField::content() const
{
    return impl(this)->content();
}

/*!
    Sets the content of the header field to \a text.
*/
void QMailMessageHeaderField::setContent(const QByteArray& text)
{
    impl(this)->setContent(text);
}

/*!
    Returns the value of the parameter with the name \a name.  
    Name comparisons are case-insensitive.
*/
QByteArray QMailMessageHeaderField::parameter(const QByteArray& name) const
{
    return impl(this)->parameter(name);
}

/*!
    Sets the parameter with the name \a name to have the value \a value, if present; 
    otherwise a new parameter is appended with the supplied properties.  If \a name
    ends with a single asterisk, the parameter will be flagged as encoded.

    \sa setParameterEncoded()
*/
void QMailMessageHeaderField::setParameter(const QByteArray& name, const QByteArray& value)
{
    impl(this)->setParameter(name, value);
}

/*!
    Returns true if the parameter with name \a name exists and is marked as encoded 
    according to RFC 2231; otherwise returns false.  
    Name comparisons are case-insensitive.
*/
bool QMailMessageHeaderField::isParameterEncoded(const QByteArray& name) const
{
    return impl(this)->isParameterEncoded(name);
}

/*!
    Sets any parameters with the name \a name to be marked as encoded.
    Name comparisons are case-insensitive.
*/
void QMailMessageHeaderField::setParameterEncoded(const QByteArray& name)
{
    impl(this)->setParameterEncoded(name);
}

/*!
    Returns the list of parameters from the header field. For each parameter, the
    member \c first contains the name text, and the member \c second contains the value text.
*/
QList<QMailMessageHeaderField::ParameterType> QMailMessageHeaderField::parameters() const
{
    return impl(this)->parameters();
}

/*!
    Returns the entire header field text as a formatted string, with the name of the field
    included if \a includeName is true.  If \a presentable is true, artifacts of RFC 2822 
    transmission format such as parameter folding will be removed.  For example: 
    
    \code
    QMailMessageHeaderField hdr;
    hdr.setId("Content-Type");
    hdr.setContent("text/plain");
    hdr.setParameter("charset", "us-ascii");

    QString s = hdr.toString();  // s: "Content-Type: text/plain; charset=us-ascii"
    \endcode
*/
QByteArray QMailMessageHeaderField::toString(bool includeName, bool presentable) const
{
    return impl(this)->toString(includeName, presentable);
}

/*!
    Returns the content of the header field as unicode text.  If the content of the
    field contains any encoded-word or encoded-parameter values, they will be decoded on output.
*/
QString QMailMessageHeaderField::decodedContent() const
{
    return impl(this)->decodedContent();
}

/*! \internal */
void QMailMessageHeaderField::parse(const QByteArray& text, FieldType fieldType)
{
    return impl(this)->parse(text, (fieldType == StructuredField));
}

/*!
    Returns the content of the string \a input encoded into a series of RFC 2045 'encoded-word'
    format tokens, each no longer than 75 characters.  The encoding used can be specified in 
    \a charset, or can be deduced from the content of \a input if \a charset is empty.
*/
QByteArray QMailMessageHeaderField::encodeWord(const QString& input, const QByteArray& charset)
{
    return ::encodeWord(input, charset, 0);
}

/*!
    Returns the content of \a input decoded from RFC 2045 'encoded-word' format.
*/
QString QMailMessageHeaderField::decodeWord(const QByteArray& input)
{
    // This could actually be a sequence of encoded words...
    return decodeWordSequence(input);
}

/*!
    Returns the content of the string \a input encoded into RFC 2231 'extended-parameter'
    format.  The encoding used can be specified in \a charset, or can be deduced from the 
    content of \a input if \a charset is empty.  If \a language is non-empty, it will be 
    included in the encoded output; otherwise the language component will be extracted from 
    \a charset, if it contains a trailing language specifier as defined in RFC 2231.
*/
QByteArray QMailMessageHeaderField::encodeParameter(const QString& input, const QByteArray& charset, const QByteArray& language)
{
    return ::encodeParameter(input, charset, language);
}

/*!
    Returns the content of \a input decoded from RFC 2231 'extended-parameter' format.
*/
QString QMailMessageHeaderField::decodeParameter(const QByteArray& input)
{
    return ::decodeParameter(input);
}

/*!
    Returns the content of the string \a input encoded into a sequence of RFC 2045 'encoded-word'
    format tokens.  The encoding used can be specified in \a charset, or can be deduced for each
    token read from \a input if \a charset is empty.
*/
QByteArray QMailMessageHeaderField::encodeContent(const QString& input, const QByteArray& charset)
{
    return encodeWordSequence(input, charset);
}

/*!
    Returns the content of \a input, decoding any encountered RFC 2045 'encoded-word' format
    tokens to unicode.
*/
QString QMailMessageHeaderField::decodeContent(const QByteArray& input)
{
    return decodeWordSequence(input);
}

/*!
    Returns the content of \a input with any comment sections removed.
*/
QByteArray QMailMessageHeaderField::removeComments(const QByteArray& input)
{
    return ::removeComments(input, &::isprint);
}

/*!
    Returns the content of \a input with any whitespace characters removed. 
    Whitespace inside double quotes is preserved.
*/
QByteArray QMailMessageHeaderField::removeWhitespace(const QByteArray& input)
{
    QByteArray result;
    result.reserve(input.length());

    const char* const begin = input.constData();
    const char* const end = begin + input.length();
    const char* it = begin;
    for (bool quoted = false; it != end; ++it) {
        if (*it == '"') {
            if ((it == begin) || (*(it - 1) != '\\'))
                quoted = !quoted;
        }
        if (quoted || !isspace(*it))
            result.append(*it);
    }
    
    return result;
}

/*! \internal */
void QMailMessageHeaderField::output(QDataStream& out) const
{
    impl(this)->output(out);
}

/*! 
    \fn QMailMessageHeaderField::serialize(Stream&) const
    \internal 
*/
template <typename Stream> 
void QMailMessageHeaderField::serialize(Stream &stream) const
{
    impl(this)->serialize(stream);
}

/*! 
    \fn QMailMessageHeaderField::deserialize(Stream&)
    \internal 
*/
template <typename Stream> 
void QMailMessageHeaderField::deserialize(Stream &stream)
{
    impl(this)->deserialize(stream);
}


/*!
    \class QMailMessageContentType
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessageContentType class encapsulates the parsing of the RFC 2822
    'Content-Type' header field.
    
    \ingroup messaginglibrary
   
    QMailMessageContentType provides simplified access to the various components of the 
    'Content-Type' header field.
    Components of the header field not exposed by member functions can be accessed using
    the functions inherited from QMailMessageHeaderField.
*/

/*! \internal */
QMailMessageContentType::QMailMessageContentType()
    : QMailMessageHeaderField("Content-Type")
{
}

/*!
    Creates a content type object from the data in \a type.
*/
QMailMessageContentType::QMailMessageContentType(const QByteArray& type)
    : QMailMessageHeaderField("Content-Type")
{
    // Find the components, and create a content value from them
    QByteArray content;

    // Although a conforming CT must be: <type> "/" <subtype> without whitespace,
    // we'll be a bit more accepting
    int index = type.indexOf('/');
    if (index == -1)
    {
        content = type.trimmed();
    }
    else
    {
        QByteArray primaryType = type.left(index).trimmed();
        QByteArray secondaryType = type.mid(index + 1).trimmed();

        content = primaryType;
        if (!secondaryType.isEmpty())
            content.append('/').append(secondaryType);
    }

    parse(content, StructuredField);
}

/*!
    Creates a content type object from the content of \a field.
*/
QMailMessageContentType::QMailMessageContentType(const QMailMessageHeaderField& field)
    : QMailMessageHeaderField(field)
{
    QMailMessageHeaderField::setId("Content-Type");
}

/*!
    Returns the primary type information of the content type header field.

    For example: if content() returns "text/plain", then type() returns "text"
*/
QByteArray QMailMessageContentType::type() const
{
    QByteArray entire = content();
    int index = entire.indexOf('/');
    if (index == -1)
        return entire.trimmed();

    return entire.left(index).trimmed();
}

/*!
    Sets the primary type information of the 'Content-Type' header field to \a type. If \a type 
    is empty, then any pre-existing sub-type information will be cleared.

    \sa setSubType()
*/
void QMailMessageContentType::setType(const QByteArray& type)
{
    if (type.isEmpty())
    {
        // Note - if there is a sub-type, setting type to null will destroy it
        setContent(type);
    }
    else
    {
        QByteArray content(type);

        QByteArray secondaryType(subType());
        if (!secondaryType.isEmpty())
            content.append('/').append(secondaryType);

        setContent(content);
    }
}

/*!
    Returns the sub-type information of the 'Content-Type' header field.

    For example: if content() returns "text/plain", then subType() returns "plain"
*/
QByteArray QMailMessageContentType::subType() const
{
    QByteArray entire = content();
    int index = entire.indexOf('/');
    if (index == -1)
        return QByteArray();

    return entire.mid(index + 1).trimmed();
}

/*!
    Sets the sub-type information of the 'Content-Type' header field to \a subType. If no primary
    type has been set, then setting the sub-type has no effect.

    \sa setType()
*/
void QMailMessageContentType::setSubType(const QByteArray& subType)
{
    QByteArray primaryType(type());
    if (!primaryType.isEmpty())
    {
        if (!subType.isEmpty())
            primaryType.append('/').append(subType);

        setContent(primaryType);
    }
}

/*!
    Returns the value of the 'name' parameter, if present; otherwise returns an empty QByteArray.
*/
QByteArray QMailMessageContentType::name() const
{
    return parameter("name");
}

/*!
    Sets the value of the 'name' parameter to \a name.
*/
void QMailMessageContentType::setName(const QByteArray& name)
{
    setParameter("name", name);
}

/*!
    Returns the value of the 'boundary' parameter, if present; otherwise returns an empty QByteArray.
*/
QByteArray QMailMessageContentType::boundary() const
{
    QByteArray value = parameter("boundary");
    if (value.isEmpty() || !isParameterEncoded("boundary"))
        return value;

    // The boundary is an encoded parameter.  Therefore, we need to extract the
    // usable ascii part, since a valid message must be composed of ascii only
    return to7BitAscii(QMailMessageHeaderField::decodeParameter(value));
}

/*!
    Sets the value of the 'boundary' parameter to \a boundary.
*/
void QMailMessageContentType::setBoundary(const QByteArray& boundary)
{
    setParameter("boundary", boundary);
}

/*!
    Returns the value of the 'charset' parameter, if present; otherwise returns an empty QByteArray.
*/
QByteArray QMailMessageContentType::charset() const
{
    QByteArray value = parameter("charset");
    if (value.isEmpty() || !isParameterEncoded("charset"))
        return value;

    // The boundary is an encoded parameter.  Therefore, we need to extract the
    // usable ascii part, since a valid charset must be composed of ascii only
    return to7BitAscii(QMailMessageHeaderField::decodeParameter(value));
}

/*!
    Sets the value of the 'charset' parameter to \a charset.
*/
void QMailMessageContentType::setCharset(const QByteArray& charset)
{
    setParameter("charset", charset);
}


/*!
    \class QMailMessageContentDisposition
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessageContentDisposition class encapsulates the parsing of the RFC 2822
    'Content-Disposition' header field.
    
    \ingroup messaginglibrary
   
    QMailMessageContentDisposition provides simplified access to the various components of the 
    'Content-Disposition' header field.
    Components of the header field not exposed by member functions can be accessed using
    the functions inherited from QMailMessageHeaderField.
*/

/*!
    \enum QMailMessageContentDisposition::DispositionType
    
    This enum type is used to describe the disposition of a message part.

    \value Attachment   The part data should be presented as an attachment.
    \value Inline       The part data should be presented inline.
    \value None         The disposition of the part is unknown.
*/

/*! \internal */
QMailMessageContentDisposition::QMailMessageContentDisposition()
    : QMailMessageHeaderField("Content-Disposition")
{
}

/*!
    Creates a disposition header field object from the data in \a type.
*/
QMailMessageContentDisposition::QMailMessageContentDisposition(const QByteArray& type)
    : QMailMessageHeaderField("Content-Disposition", type)
{
}

/*!
    Creates a 'Content-Disposition' header field object with the type \a type.
*/
QMailMessageContentDisposition::QMailMessageContentDisposition(QMailMessageContentDisposition::DispositionType type)
    : QMailMessageHeaderField("Content-Disposition")
{
    setType(type);
}

/*!
    Creates a disposition header field object from the content of \a field.
*/
QMailMessageContentDisposition::QMailMessageContentDisposition(const QMailMessageHeaderField& field)
    : QMailMessageHeaderField(field)
{
    QMailMessageHeaderField::setId("Content-Disposition");
}

/*!
    Returns the disposition type of this header field.
*/
QMailMessageContentDisposition::DispositionType QMailMessageContentDisposition::type() const
{
    const QByteArray& type = content();

    if (insensitiveEqual(type, "inline"))
        return Inline;
    else if (insensitiveEqual(type, "attachment"))
        return Attachment;

    return None;
}

/*!
    Sets the disposition type of this field to \a type.
*/
void QMailMessageContentDisposition::setType(QMailMessageContentDisposition::DispositionType type)
{
    if (type == Inline)
        setContent("inline");
    else if (type == Attachment)
        setContent("attachment");
    else
        setContent(QByteArray());
}

/*!
    Returns the value of the 'filename' parameter, if present; otherwise returns an empty QByteArray.
*/
QByteArray QMailMessageContentDisposition::filename() const
{
    return parameter("filename");
}

/*!
    Sets the value of the 'filename' parameter to \a filename.
*/
void QMailMessageContentDisposition::setFilename(const QByteArray& filename)
{
    setParameter("filename", filename);
}

/*!
    Returns the value of the 'creation-date' parameter, if present; otherwise returns an uninitialised time stamp.
*/
QMailTimeStamp QMailMessageContentDisposition::creationDate() const
{
    return QMailTimeStamp(parameter("creation-date"));
}

/*!
    Sets the value of the 'creation-date' parameter to \a timeStamp.
*/
void QMailMessageContentDisposition::setCreationDate(const QMailTimeStamp& timeStamp)
{
    setParameter("creation-date", to7BitAscii(timeStamp.toString()));
}

/*!
    Returns the value of the 'modification-date' parameter, if present; otherwise returns an uninitialised time stamp.
*/
QMailTimeStamp QMailMessageContentDisposition::modificationDate() const
{
    return QMailTimeStamp(parameter("modification-date"));
}

/*!
    Sets the value of the 'modification-date' parameter to \a timeStamp.
*/
void QMailMessageContentDisposition::setModificationDate(const QMailTimeStamp& timeStamp)
{
    setParameter("modification-date", to7BitAscii(timeStamp.toString()));
}


/*!
    Returns the value of the 'read-date' parameter, if present; otherwise returns an uninitialised time stamp.
*/
QMailTimeStamp QMailMessageContentDisposition::readDate() const
{
    return QMailTimeStamp(parameter("read-date"));
}

/*!
    Sets the value of the 'read-date' parameter to \a timeStamp.
*/
void QMailMessageContentDisposition::setReadDate(const QMailTimeStamp& timeStamp)
{
    setParameter("read-date", to7BitAscii(timeStamp.toString()));
}

/*!
    Returns the value of the 'size' parameter, if present; otherwise returns -1.
*/
int QMailMessageContentDisposition::size() const
{
    QByteArray sizeText = parameter("size");

    if (sizeText.isEmpty())
        return -1;

    return sizeText.toUInt();
}

/*!
    Sets the value of the 'size' parameter to \a size.
*/
void QMailMessageContentDisposition::setSize(int size)
{
    setParameter("size", QByteArray::number(size));
}


/* QMailMessageHeader*/
class QMailMessageHeaderPrivate : public QPrivateImplementationBase
{
public:
    QMailMessageHeaderPrivate();
    QMailMessageHeaderPrivate(const QByteArray& input);

    void update(const QByteArray &id, const QByteArray &content);
    void append(const QByteArray &id, const QByteArray &content);
    void remove(const QByteArray &id);

    QList<QMailMessageHeaderField> fields(const QByteArray& id, int maximum = -1) const;

    void output(QDataStream& out, const QList<QByteArray>& exclusions, bool stripInternal) const;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessageHeader;

    QList<QByteArray> _headerFields;
};

QMailMessageHeaderPrivate::QMailMessageHeaderPrivate()
    : QPrivateImplementationBase(this)
{
}

enum NewLineStatus { None, Cr, CrLf };

static QList<QByteArray> parseHeaders(const QByteArray& input)
{
    QList<QByteArray> result;
    QByteArray progress;

    // Find each terminating newline, which must be CR, LF, then non-whitespace or end
    NewLineStatus status = None;

    const char* begin = input.constData();
    const char* it = begin;
    for (const char* const end = it + input.length(); it != end; ++it) {
        if (status == CrLf) {
            if (*it == ' ' || *it == '\t') {
                // The CRLF was folded
                if ((it - begin) > 2) {
                    progress.append(QByteArray(begin, (it - begin - 2)));
                }
                begin = it;
            }
            else {
                // That was an unescaped CRLF
                if ((it - begin) > 2) {
                    progress.append(QByteArray(begin, (it - begin) - 2));
                }
                if (!progress.isEmpty()) {
                    // Non-empty field
                    result.append(progress);
                    progress.clear();
                }
                begin = it;
            }
            status = None;
        }
        else if (status == Cr) {
            if (*it == QMailMessage::LineFeed) {
                // CRLF sequence completed
                status = CrLf;
            }
            else {
                status = None;
            }
        }
        else {
            if (*it == QMailMessage::CarriageReturn)
                status = Cr;
        }
    }

    if (it != begin) {
        int skip = (status == CrLf ? 2 : (status == None ? 0 : 1));
        if ((it - begin) > skip) {
            progress.append(QByteArray(begin, (it - begin) - skip));
        }
        if (!progress.isEmpty()) {
            result.append(progress);
        }
    }

    return result;
}

QMailMessageHeaderPrivate::QMailMessageHeaderPrivate(const QByteArray& input)
    : QPrivateImplementationBase(this),
      _headerFields(parseHeaders(input))
{
}

static QByteArray fieldId(const QByteArray &id)
{
    QByteArray name = id.trimmed();
    if ( !name.endsWith(':') )
        name.append(':');
    return name;
}

static QPair<QByteArray, QByteArray> fieldParts(const QByteArray &id, const QByteArray &content)
{
    QByteArray value = QByteArray(" ") + content.trimmed();
    return qMakePair(fieldId(id), value);
}

static bool matchingId(const QByteArray& id, const QByteArray& other, bool allowPartial = false)
{
    QByteArray match(id.trimmed());

    int index = insensitiveIndexOf(match, other);
    if (index == -1)
        return false;

    if (index > 0)
    {
        // Ensure that every preceding character is whitespace
        QByteArray leader(other.left(index).trimmed());
        if (!leader.isEmpty())
            return false;
    }

    if (allowPartial)
        return true;

    int lastIndex = index + match.length() - 1;
    index = other.indexOf(':', lastIndex);
    if (index == -1)
        index = other.length() - 1;

    // Ensure that there is only whitespace between the matched ID and the end of the ID
    if ((index - lastIndex) > 1)
    {
        QByteArray trailer(other.mid(lastIndex + 1, (index - lastIndex)).trimmed());
        if (!trailer.isEmpty())
            return false;
    }

    return true;
}

void QMailMessageHeaderPrivate::update(const QByteArray &id, const QByteArray &content)
{
    QPair<QByteArray, QByteArray> parts = fieldParts(id, content);
    QByteArray updated = parts.first + parts.second;

    const QList<QByteArray>::Iterator end = _headerFields.end();
    for (QList<QByteArray>::Iterator it = _headerFields.begin(); it != end; ++it) {
        if ( matchingId(id, (*it)) ) {
            *it = updated;
            return;
        }
    }

    // new header field, add it
    _headerFields.append( updated );
}

void QMailMessageHeaderPrivate::append(const QByteArray &id, const QByteArray &content)
{
    QPair<QByteArray, QByteArray> parts = fieldParts(id, content);
    _headerFields.append( parts.first + parts.second );
}

void QMailMessageHeaderPrivate::remove(const QByteArray &id)
{
    QList<QList<QByteArray>::Iterator> matches;

    const QList<QByteArray>::Iterator end = _headerFields.end();
    for (QList<QByteArray>::Iterator it = _headerFields.begin(); it != end; ++it) {
        if ( matchingId(id, (*it)) )
            matches.prepend(it);
    }

    foreach (QList<QByteArray>::Iterator it, matches)
        _headerFields.erase(it);
}

QList<QMailMessageHeaderField> QMailMessageHeaderPrivate::fields(const QByteArray& id, int maximum) const
{
    QList<QMailMessageHeaderField> result;

    foreach (const QByteArray& field, _headerFields) {
        QMailMessageHeaderField headerField(field, QMailMessageHeaderField::UnstructuredField);
        if ( matchingId(id, headerField.id()) ) {
            result.append(headerField);
            if (maximum > 0 && result.count() == maximum)
                return result;
        }
    }

    return result;
}

void QMailMessageHeaderPrivate::output(QDataStream& out, const QList<QByteArray>& exclusions, bool stripInternal) const
{
    foreach (const QByteArray& field, _headerFields) {
        QMailMessageHeaderField headerField(field, QMailMessageHeaderField::UnstructuredField);
        const QByteArray& id = headerField.id();
        bool excluded = false;

        // Bypass any header field that has the internal prefix
        if (stripInternal)
            excluded = matchingId(internalPrefix(), id, true);

        // Bypass any header in the list of exclusions
        if (!excluded)
            foreach (const QByteArray& exclusion, exclusions)
                if (matchingId(exclusion, id))
                    excluded = true;

        if (!excluded)
            headerField.output(out);
    }
}

template <typename Stream> 
void QMailMessageHeaderPrivate::serialize(Stream &stream) const
{
    stream << _headerFields;
}

template <typename Stream> 
void QMailMessageHeaderPrivate::deserialize(Stream &stream)
{
    stream >> _headerFields;
}

template class QPrivatelyImplemented<QMailMessageHeaderPrivate>;


/*!
    \class QMailMessageHeader
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule
    \internal
*/

QMailMessageHeader::QMailMessageHeader()
    : QPrivatelyImplemented<QMailMessageHeaderPrivate>(new QMailMessageHeaderPrivate())
{
}

QMailMessageHeader::QMailMessageHeader(const QByteArray& input)
    : QPrivatelyImplemented<QMailMessageHeaderPrivate>(new QMailMessageHeaderPrivate(input))
{
}

void QMailMessageHeader::update(const QByteArray &id, const QByteArray &content)
{
    impl(this)->update(id, content);
}

void QMailMessageHeader::append(const QByteArray &id, const QByteArray &content)
{
    impl(this)->append(id, content);
}

void QMailMessageHeader::remove(const QByteArray &id)
{
    impl(this)->remove(id);
}

QMailMessageHeaderField QMailMessageHeader::field(const QByteArray& id) const
{
    QList<QMailMessageHeaderField> result = impl(this)->fields(id, 1);
    if (result.count())
        return result[0];

    return QMailMessageHeaderField();
}

QList<QMailMessageHeaderField> QMailMessageHeader::fields(const QByteArray& id) const
{
    return impl(this)->fields(id);
}

QList<const QByteArray*> QMailMessageHeader::fieldList() const
{
    QList<const QByteArray*> result;

    QList<QByteArray>::ConstIterator const end = impl(this)->_headerFields.end();
    for (QList<QByteArray>::ConstIterator it = impl(this)->_headerFields.begin(); it != end; ++it)
        result.append(&(*it));

    return result;
}

void QMailMessageHeader::output(QDataStream& out, const QList<QByteArray>& exclusions, bool stripInternal) const
{
    impl(this)->output(out, exclusions, stripInternal);
}

/*! 
    \fn QMailMessageHeader::serialize(Stream&) const
    \internal 
*/
template <typename Stream> 
void QMailMessageHeader::serialize(Stream &stream) const
{
    impl(this)->serialize(stream);
}

/*! 
    \fn QMailMessageHeader::deserialize(Stream&)
    \internal 
*/
template <typename Stream> 
void QMailMessageHeader::deserialize(Stream &stream)
{
    impl(this)->deserialize(stream);
}


/* QMailMessageBody */
class QMailMessageBodyPrivate : public QPrivateImplementationBase
{
public:
    QMailMessageBodyPrivate();

    void fromLongString(LongString& ls, const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding, QMailMessageBody::EncodingStatus status);
    void fromFile(const QString& filename, const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding, QMailMessageBody::EncodingStatus status);
    void fromStream(QDataStream& in, const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding, QMailMessageBody::EncodingStatus status);
    void fromStream(QTextStream& in, const QMailMessageContentType& type, QMailMessageBody::TransferEncoding encoding);

    bool toFile(const QString& filename, QMailMessageBody::EncodingFormat format) const;
    bool toStream(QDataStream& out, QMailMessageBody::EncodingFormat format) const;
    bool toStream(QTextStream& out) const;

    QMailMessageBody::TransferEncoding transferEncoding() const;
    QMailMessageContentType contentType() const;

    uint indicativeSize() const;

    void output(QDataStream& out, bool includeAttachments) const;

    // We will express the indicative size of the body in units of this:
    static const uint IndicativeSizeUnit = 2048;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessageBody;

    QMailMessageBody::TransferEncoding _encoding;
    LongString _bodyData;
    QString _filename;
    bool _encoded;
    QMailMessageContentType _type;
};


QMailMessageBodyPrivate::QMailMessageBodyPrivate()
    : QPrivateImplementationBase(this)
{
    // Default encoding
    _encoding = QMailMessageBody::SevenBit;
}

void QMailMessageBodyPrivate::fromLongString(LongString& ls, const QMailMessageContentType& content, QMailMessageBody::TransferEncoding te, QMailMessageBody::EncodingStatus status)
{
    _encoding = te;
    _type = content;
    _encoded = (status == QMailMessageBody::AlreadyEncoded);
    _filename = QString();
    _bodyData = ls;
}

void QMailMessageBodyPrivate::fromFile(const QString& file, const QMailMessageContentType& content, QMailMessageBody::TransferEncoding te, QMailMessageBody::EncodingStatus status)
{
    _encoding = te;
    _type = content;
    _encoded = (status == QMailMessageBody::AlreadyEncoded);
    _filename = file;
    _bodyData = LongString(file);
}

void QMailMessageBodyPrivate::fromStream(QDataStream& in, const QMailMessageContentType& content, QMailMessageBody::TransferEncoding te, QMailMessageBody::EncodingStatus status)
{
    _encoding = te;
    _type = content;
    _encoded = true;
    _filename = QString();
    _bodyData = LongString();
    
    // If the data is already encoded, we don't need to do it again
    if (status == QMailMessageBody::AlreadyEncoded)
        te = QMailMessageBody::SevenBit;

    QMailCodec* codec = codecForEncoding(te, content);
    if (codec)
    {
        // Stream to the buffer, encoding as required
        QByteArray encoded;
        {
            QDataStream out(&encoded, QIODevice::WriteOnly);
            codec->encode(out, in);
        }
        _bodyData = LongString(encoded);
        delete codec;
    }
}

void QMailMessageBodyPrivate::fromStream(QTextStream& in, const QMailMessageContentType& content, QMailMessageBody::TransferEncoding te)
{
    _encoding = te;
    _type = content;
    _encoded = true;
    _filename = QString();
    _bodyData = LongString();

    QMailCodec* codec = codecForEncoding(te, content);
    if (codec)
    {
        QByteArray encoded;
        {
            QDataStream out(&encoded, QIODevice::WriteOnly);

            // Convert the unicode string to a byte-stream, via the nominated character set
            QString charset = _type.charset();

            // If no character set is specified - treat the data as UTF-8; since it is
            // textual data, it must have some character set...
            if (charset.isEmpty())
                charset = "UTF-8";

            codec->encode(out, in, charset);
        }
        _bodyData = LongString(encoded);
        delete codec;
    }
}

static bool unicodeConvertingCharset(const QByteArray& charset)
{
    // See if this is a unicode-capable codec
    if (QTextCodec* textCodec = codecForName(charset, true))
    {
        const QChar multiByteChar = 0x1234;
        return textCodec->canEncode(multiByteChar);
    }
    else
    {
        qWarning() << "unicodeConvertingCharset: unable to find codec for charset:" << charset;
    }

    return false;
}

static QByteArray extractionCharset(const QMailMessageContentType& type)
{
    QByteArray charset;

    // Find the charset for this data, if it is text data
    if (insensitiveEqual(type.type(), "text"))
    {
        charset = type.charset();
        if (!charset.isEmpty())
        {
            // If the codec can't handle multi-byte characters, don't extract to/from unicode
            if (!unicodeConvertingCharset(charset))
                charset = QByteArray();
        }
    }

    return charset;
}

bool QMailMessageBodyPrivate::toFile(const QString& file, QMailMessageBody::EncodingFormat format) const
{
    QFile outFile(file);
    if (!outFile.open(QIODevice::WriteOnly))
    {
        qWarning() << "Unable to open for write:" << file;
        return false;
    }

    bool encodeOutput = (format == QMailMessageBody::Encoded);

    // Find the charset for this data, if it is text data
    QByteArray charset(extractionCharset(_type));

    QMailMessageBody::TransferEncoding te = _encoding;

    // If our data is in the required condition, we don't need to encode/decode
    if (encodeOutput == _encoded)
        te = QMailMessageBody::Binary;

    QMailCodec* codec = codecForEncoding(te, _type);
    if (codec)
    {
        bool result = false;

        // Empty charset indicates no unicode encoding; encoded return data means binary streams
        if (charset.isEmpty() || encodeOutput)
        {
            // We are dealing with binary data
            QDataStream out(&outFile);
            QDataStream* in = _bodyData.dataStream();
            if (encodeOutput)
                codec->encode(out, *in);
            else
                codec->decode(out, *in);
            result = (in->status() == QDataStream::Ok);
            delete in;
        }
        else // we should probably check that charset matches this->charset
        {
            // We are dealing with unicode text data, which we want in unencoded form
            QTextStream out(&outFile);

            // If the content is unencoded we can pass it back via a text stream
            if (!_encoded)
            {
                QTextStream* in = _bodyData.textStream();
                QMailCodec::copy(out, *in);
                result = (in->status() == QTextStream::Ok);
                delete in;
            }
            else
            {
                QDataStream* in = _bodyData.dataStream();
                codec->decode(out, *in, charset);
                result = (in->status() == QDataStream::Ok);
                delete in;
            }
        }

        delete codec;
        return result;
    }

    return false;
}

bool QMailMessageBodyPrivate::toStream(QDataStream& out, QMailMessageBody::EncodingFormat format) const
{
    bool encodeOutput = (format == QMailMessageBody::Encoded);
    QMailMessageBody::TransferEncoding te = _encoding;

    // If our data is in the required condition, we don't need to encode/decode
    if (encodeOutput == _encoded)
        te = QMailMessageBody::Binary;

    QMailCodec* codec = codecForEncoding(te, _type);
    if (codec)
    {
        bool result = false;

        QByteArray charset(extractionCharset(_type));
        if (!charset.isEmpty() && !_filename.isEmpty() && encodeOutput)
        {
            // This data must be unicode in the file
            QTextStream* in = _bodyData.textStream();
            codec->encode(out, *in, charset);
            result = (in->status() == QTextStream::Ok);
            delete in;
        }
        else
        {
            QDataStream* in = _bodyData.dataStream();
            if (encodeOutput)
                codec->encode(out, *in);
            else
                codec->decode(out, *in);
            result = (in->status() == QDataStream::Ok);
            delete in;
        }

        delete codec;
        return result;
    }

    return false;
}

bool QMailMessageBodyPrivate::toStream(QTextStream& out) const
{
    QByteArray charset = _type.charset();
    if (charset.isEmpty() || (insensitiveIndexOf("ascii", charset) != -1)) {
        // We'll assume the text is plain ASCII, to be extracted to Latin-1
        charset = "ISO-8859-1";
    }

    QMailMessageBody::TransferEncoding te = _encoding;

    // If our data is not encoded, we don't need to decode
    if (!_encoded)
        te = QMailMessageBody::Binary;

    QMailCodec* codec = codecForEncoding(te, _type);
    if (codec)
    {
        bool result = false;

        if (!_encoded && !_filename.isEmpty() && unicodeConvertingCharset(charset))
        { 
            // The data is already in unicode format
            QTextStream* in = _bodyData.textStream();
            QMailCodec::copy(out, *in);
            result = (in->status() == QTextStream::Ok);
            delete in;
        }
        else
        {
            // Write the data to out, decoding if necessary
            QDataStream* in = _bodyData.dataStream();
            codec->decode(out, *in, charset);
            result = (in->status() == QDataStream::Ok);
            delete in;
        }

        delete codec;
        return result;
    }

    return false;
}

QMailMessageContentType QMailMessageBodyPrivate::contentType() const
{
    return _type;
}

QMailMessageBody::TransferEncoding QMailMessageBodyPrivate::transferEncoding() const
{
    return _encoding;
}

uint QMailMessageBodyPrivate::indicativeSize() const
{
    // Treat the body as a part at least comparable to the header block.  We need to
    // differentiate between small and large parts, but the size difference should be 
    // underestimated, since most of the latency is borne by the header (the protocols
    // require per-message exchanges).
    return ((_bodyData.length() / IndicativeSizeUnit) + 1);
}

void QMailMessageBodyPrivate::output(QDataStream& out, bool includeAttachments) const
{
    if ( includeAttachments )
        toStream( out, QMailMessageBody::Encoded );
}

template <typename Stream> 
void QMailMessageBodyPrivate::serialize(Stream &stream) const
{
    stream << _encoding;
    stream << _bodyData;
    stream << _filename;
    stream << _encoded;
    stream << _type;
}

template <typename Stream> 
void QMailMessageBodyPrivate::deserialize(Stream &stream)
{
    stream >> _encoding;
    stream >> _bodyData;
    stream >> _filename;
    stream >> _encoded;
    stream >> _type;
}

template class QPrivatelyImplemented<QMailMessageBodyPrivate>;


/*!
    \class QMailMessageBody
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessageBody class contains the body element of a message or message part.
    
    \ingroup messaginglibrary
   
    The body of a message or message part is treated as an atomic unit by the Qt Extended messaging library.  It can only be inserted into a message part container or extracted
    from one.  It can be inserted or extracted using either a QByteArray, a QDataStream
    or to/from a file.  In the case of unicode text data, the insertion and extraction can
    operate on either a QString, a QTextStream or to/from a file.

    The body data must be associated with a QMailMessageContentType describing that data.
    When extracting body data from a message or part to unicode text, the content type
    description must include a parameter named 'charset'; this parameter is used to locate
    a QTextCodec to be used to extract unicode data from the body data octet stream.  
    
    If the Content-Type of the data is a subtype of "text", then line-ending translation 
    will be used to ensure that the text is transmitted with CR/LF line endings.  The text 
    data supplied to QMailMessageBody must conform to the RFC 2822 restrictions on maximum 
    line lengths: "Each line of characters MUST be no more than 998 characters, and SHOULD 
    be no more than 78 characters, excluding the CRLF."  Textual message body data decoded 
    from a QMailMessageBody object will have transmitted CR/LF line endings converted to 
    \c \n on extraction.

    The body data can also be encoded from 8-bit octets to 7-bit ASCII characters for
    safe transmission through obsolete email systems.  When creating an instance of the 
    QMailMessageBody class, the encoding to be used must be specified using the 
    QMailMessageBody::TransferEncoding enum.

    \sa QMailMessagePart, QMailMessage, QTextCodec
*/    

/*!
    \typedef QMailMessageBody::ImplementationType
    \internal
*/

/*!
    \enum QMailMessageBody::TransferEncoding
    
    This enum type is used to describe a type of binary to text encoding.
    Encoding types used here are documented in 
    \l {http://www.ietf.org/rfc/rfc2045.txt}{RFC 2045} "Format of Internet Message Bodies"

    \value NoEncoding          The encoding is not specified.
    \value SevenBit            The data is not encoded, but contains only 7-bit ASCII data.
    \value EightBit            The data is not encoded, but contains data using only 8-bit characters which form a superset of ASCII.
    \value Base64              A 65-character subset of US-ASCII is used, enabling 6 bits to be represented per printable character.
    \value QuotedPrintable     A method of encoding that tends to leave text similar to US-ASCII unmodified for readability.
    \value Binary              The data is not encoded to any limited subset of octet values.

    \sa QMailCodec
*/

/*!
    \enum QMailMessageBody::EncodingStatus
    
    This enum type is used to describe the encoding status of body data.

    \value AlreadyEncoded       The body data is already encoded to the necessary encoding.
    \value RequiresEncoding     The body data is unencoded, and thus requires encoding for transmission.
*/

/*!
    \enum QMailMessageBody::EncodingFormat
    
    This enum type is used to describe the format in which body data should be presented.

    \value Encoded      The body data should be presented in encoded form. 
    \value Decoded      The body data should be presented in unencoded form. 
*/

/*! 
    Creates an instance of QMailMessageBody.
*/
QMailMessageBody::QMailMessageBody()
    : QPrivatelyImplemented<QMailMessageBodyPrivate>(new QMailMessageBodyPrivate())
{
}

/*!
    Creates a message body from the data contained in the file \a filename, having the content type 
    \a type.  If \a status is QMailMessageBody::RequiresEncoding, the data from the file will be 
    encoded to \a encoding for transmission; otherwise it must already be in that encoding, which
    will be reported to recipients of the data.

    If \a type is a subtype of "text", the data will be treated as text, and line-ending
    translation will be employed.  Otherwise, the file will be treated as containing binary 
    data.  If the file contains unicode text data, it will be converted to an octet stream using
    a QTextCodec object identified by the 'charset' parameter of \a type.

    If \a encoding is QMailMessageBody::QuotedPrintable, encoding will be performed assuming
    conformance to RFC 2045.

    Note that the data is not actually read from the file until it is requested by another function.

    \sa QMailCodec, QMailQuotedPrintableCodec, QMailMessageContentType, QTextCodec
*/
QMailMessageBody QMailMessageBody::fromFile(const QString& filename, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status)
{
    QMailMessageBody body;
    body.impl<QMailMessageBodyPrivate>()->fromFile(filename, type, encoding, status);
    return body;
}

/*!
    Creates a message body from the data read from \a in, having the content type \a type.  
    If \a status is QMailMessageBody::RequiresEncoding, the data from the file will be 
    encoded to \a encoding for transmission; otherwise it must already be in that encoding, 
    which will be reported to recipients of the data.

    If \a type is a subtype of "text", the data will be treated as text, and line-ending
    translation will be employed.  Otherwise, the file will be treated as containing binary 
    data.

    If \a encoding is QMailMessageBody::QuotedPrintable, encoding will be performed assuming
    conformance to RFC 2045.

    \sa QMailCodec, QMailQuotedPrintableCodec
*/
QMailMessageBody QMailMessageBody::fromStream(QDataStream& in, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status)
{
    QMailMessageBody body;
    body.impl<QMailMessageBodyPrivate>()->fromStream(in, type, encoding, status);
    return body;
}

/*!
    Creates a message body from the data contained in \a input, having the content type 
    \a type.  If \a status is QMailMessageBody::RequiresEncoding, the data from the file will be 
    encoded to \a encoding for transmission; otherwise it must already be in that encoding, 
    which will be reported to recipients of the data.

    If \a type is a subtype of "text", the data will be treated as text, and line-ending
    translation will be employed.  Otherwise, the file will be treated as containing binary 
    data.

    If \a encoding is QMailMessageBody::QuotedPrintable, encoding will be performed assuming
    conformance to RFC 2045.

    \sa QMailCodec, QMailQuotedPrintableCodec
*/
QMailMessageBody QMailMessageBody::fromData(const QByteArray& input, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status)
{
    QMailMessageBody body;
    {
        QDataStream in(input);
        body.impl<QMailMessageBodyPrivate>()->fromStream(in, type, encoding, status);
    }
    return body;
}

/*!
    Creates a message body from the data read from \a in, having the content type \a type.  
    The data read from \a in will be encoded to \a encoding for transmission, and line-ending
    translation will be employed.  The unicode text data will be converted to an octet stream 
    using a QTextCodec object identified by the 'charset' parameter of \a type.

    If \a encoding is QMailMessageBody::QuotedPrintable, encoding will be performed assuming
    conformance to RFC 2045.

    \sa QMailCodec, QMailQuotedPrintableCodec, QMailMessageContentType, QTextCodec
*/
QMailMessageBody QMailMessageBody::fromStream(QTextStream& in, const QMailMessageContentType& type, TransferEncoding encoding)
{
    QMailMessageBody body;
    body.impl<QMailMessageBodyPrivate>()->fromStream(in, type, encoding);
    return body;
}

/*!
    Creates a message body from the data contained in \a input, having the content type 
    \a type.  The data from \a input will be encoded to \a encoding for transmission, and 
    line-ending translation will be employed.  The unicode text data will be converted to 
    an octet stream using a QTextCodec object identified by the 'charset' parameter of \a type.

    If \a encoding is QMailMessageBody::QuotedPrintable, encoding will be performed assuming
    conformance to RFC 2045.

    \sa QMailCodec, QMailMessageContentType, QTextCodec
*/
QMailMessageBody QMailMessageBody::fromData(const QString& input, const QMailMessageContentType& type, TransferEncoding encoding)
{
    QMailMessageBody body;
    {
        QTextStream in(const_cast<QString*>(&input), QIODevice::ReadOnly);
        body.impl<QMailMessageBodyPrivate>()->fromStream(in, type, encoding);
    }
    return body;
}

QMailMessageBody QMailMessageBody::fromLongString(LongString& ls, const QMailMessageContentType& type, TransferEncoding encoding, EncodingStatus status)
{
    QMailMessageBody body;
    {
        body.impl<QMailMessageBodyPrivate>()->fromLongString(ls, type, encoding, status);
    }
    return body;
}

/*!
    Writes the data of the message body to the file named \a filename.  If \a format is
    QMailMessageBody::Encoded, then the data is written in the transfer encoding it was
    created with; otherwise, it is written in unencoded form.

    If the body has a content type with a QMailMessageContentType::type() of "text", and the 
    content type parameter 'charset' is not empty, then the unencoded data will be written 
    as unicode text data, using the charset parameter to locate the appropriate QTextCodec.

    Returns false if the operation causes an error; otherwise returns true.

    \sa QMailCodec, QMailMessageContentType, QTextCodec
*/
bool QMailMessageBody::toFile(const QString& filename, EncodingFormat format) const
{
    return impl(this)->toFile(filename, format);
}

/*!
    Returns the data of the message body as a QByteArray.  If \a format is
    QMailMessageBody::Encoded, then the data is written in the transfer encoding it was
    created with; otherwise, it is written in unencoded form.

    \sa QMailCodec
*/
QByteArray QMailMessageBody::data(EncodingFormat format) const
{
    QByteArray result;
    {
        QDataStream out(&result, QIODevice::WriteOnly);
        impl(this)->toStream(out, format);
		
    }
    return result;
}

/*!
    Writes the data of the message body to the stream \a out. If \a format is
    QMailMessageBody::Encoded, then the data is written in the transfer encoding it was
    created with; otherwise, it is written in unencoded form.

    Returns false if the operation causes an error; otherwise returns true.

    \sa QMailCodec
*/
bool QMailMessageBody::toStream(QDataStream& out, EncodingFormat format) const
{
    return impl(this)->toStream(out, format);
}

/*!
    Returns the data of the message body as a QString, in unencoded form.  Line-endings
    transmitted as CR/LF pairs are converted to \c \n on extraction.

    The 'charset' parameter of the body's content type is used to locate the appropriate 
    QTextCodec to convert the data from an octet stream to unicode, if necessary.

    \sa QMailCodec, QMailMessageContentType, QTextCodec
*/
QString QMailMessageBody::data() const
{
    QString result;
    {
        QTextStream out(&result, QIODevice::WriteOnly);
        impl(this)->toStream(out);
    }
    return result;
}

/*!
    Writes the data of the message body to the stream \a out, in unencoded form. 
    Line-endings transmitted as CR/LF pairs are converted to \c \n on extraction.
    Returns false if the operation causes an error; otherwise returns true.

    The 'charset' parameter of the body's content type is used to locate the appropriate 
    QTextCodec to convert the data from an octet stream to unicode, if necessary.

    \sa QMailCodec, QMailMessageContentType, QTextCodec
*/
bool QMailMessageBody::toStream(QTextStream& out) const
{
    return impl(this)->toStream(out);
}

/*!
    Returns the content type that the body was created with.
*/
QMailMessageContentType QMailMessageBody::contentType() const
{
    return impl(this)->contentType();
}

/*!
    Returns the transfer encoding type that the body was created with.
*/
QMailMessageBody::TransferEncoding QMailMessageBody::transferEncoding() const
{
    return impl(this)->transferEncoding();
}

/*! \internal */
uint QMailMessageBody::indicativeSize() const
{
    return impl(this)->indicativeSize();
}

/*! \internal */
void QMailMessageBody::output(QDataStream& out, bool includeAttachments) const
{
    impl(this)->output(out, includeAttachments);
}

/*! 
    \fn QMailMessageBody::serialize(Stream&) const
    \internal 
*/
template <typename Stream> 
void QMailMessageBody::serialize(Stream &stream) const
{
    impl(this)->serialize(stream);
}

/*! 
    \fn QMailMessageBody::deserialize(Stream&)
    \internal 
*/
template <typename Stream> 
void QMailMessageBody::deserialize(Stream &stream)
{
    impl(this)->deserialize(stream);
}


/* QMailMessagePartContainer */
class QMailMessagePartContainerPrivate : public QPrivateImplementationBase
{
public:
    template<typename Derived>
    QMailMessagePartContainerPrivate(Derived* p);

    void setHeader(const QMailMessageHeader& header, const QMailMessagePartContainerPrivate* parent = 0);

    QByteArray headerField( const QByteArray &headerName ) const;
    QList<QByteArray> headerFields( const QByteArray &headerName, int maximum = 0 ) const;

    QList<QByteArray> headerFields() const;

    void updateHeaderField(const QByteArray &id, const QByteArray &content);
    void updateHeaderField(const QByteArray &id, const QString &content);

    void appendHeaderField(const QByteArray &id, const QByteArray &content);
    void appendHeaderField(const QByteArray &id, const QString &content);

    void removeHeaderField(const QByteArray &id);

    void setMultipartType(QMailMessagePartContainer::MultipartType type);
    void appendPart(const QMailMessagePart &part);
    void prependPart(const QMailMessagePart &part);
    void clear();

    QMailMessageContentType contentType() const;
    QMailMessageBody::TransferEncoding transferEncoding() const;

    uint indicativeSize() const;

protected:
    friend class QMailMessagePartContainer;
    friend class QMailMessagePrivate;

    void defaultContentType(const QMailMessagePartContainerPrivate* parent);

    void outputParts(QDataStream& out, bool includePreamble, bool includeAttachments, bool stripInternal) const;
    void outputBody(QDataStream& out, bool includeAttachments) const;

    QString headerFieldText( const QString &id ) const;

    QByteArray boundary() const;
    void setBoundary(const QByteArray& text);

    // Note: this returns a reference:
    QMailMessageBody& body();
    const QMailMessageBody& body() const;
    void setBody(const QMailMessageBody& body);

    bool hasBody() const;

    void parseMimeSinglePart(const QMailMessageHeader& partHeader, LongString body, int partNumber);
    void parseMimeMultipart(const QMailMessageHeader& partHeader, LongString body, int partNumber, bool insertIntoSelf);
    bool parseMimePart(LongString body, int partNumber);

    static QMailMessagePartContainerPrivate* privatePointer(QMailMessagePart& part);

    bool dirty() const;
    void setDirty(bool value = true, bool recursive = false);

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

    QMailMessagePartContainer::MultipartType _multipartType;
    QList<QMailMessagePart> _messageParts;
    mutable QByteArray _boundary;
    QMailMessageHeader _header;
    QMailMessageBody _body;
    bool _hasBody;
    bool _dirty;
};


// Few content messages will contain a sequence of unmatched braces?
static const QByteArray uninitialisedBoundary = "[}<}]";

template<typename Derived>
QMailMessagePartContainerPrivate::QMailMessagePartContainerPrivate(Derived* p)
    : QPrivateImplementationBase(p)
{
    _multipartType = QMailMessagePartContainer::MultipartNone;
    _hasBody = false;
    _dirty = false;
}

void QMailMessagePartContainerPrivate::setHeader(const QMailMessageHeader& partHeader, const QMailMessagePartContainerPrivate* parent)
{
    _header = partHeader;

    defaultContentType(parent);

    QByteArray contentType = headerField("Content-Type");
    if (!contentType.isEmpty())
    {
        // Extract the stored parts from the supplied field
        QMailMessageContentType type(contentType);
        _multipartType = multipartTypeForName(type.content());
        _boundary = type.boundary();
    }
}

void QMailMessagePartContainerPrivate::defaultContentType(const QMailMessagePartContainerPrivate* parent)
{
    QMailMessageContentType type;

    // Find the content-type, or use default values
    QByteArray contentType = headerField("Content-Type");
    bool useDefault = contentType.isEmpty();

    if (!useDefault)
    {
        type = QMailMessageContentType(contentType);

        if (type.type().isEmpty() || type.subType().isEmpty())
        {
            useDefault = true;
        }
        else if (insensitiveEqual(type.content(), "application/octet-stream"))
        {
            // Sender's client might not know what type, but maybe we do. Try...
            QByteArray contentDisposition = headerField("Content-Disposition");
            if (!contentDisposition.isEmpty())
            {
                QMailMessageContentDisposition disposition(contentDisposition);

                QString mimeType = QMimeType(disposition.filename()).id();
                if (!mimeType.isEmpty()) 
                {
                    type.setContent(to7BitAscii(mimeType));
                    updateHeaderField(type.id(), type.toString(false, false));
                }
            }
        }
    }

    if (useDefault && parent)
    {
        // Note that the default is 'message/rfc822' when the parent is 'multipart/digest'
        QMailMessageContentType parentType = parent->contentType();
        if (parentType.content().toLower() == "multipart/digest")
        {
            type.setType("message");
            type.setSubType("rfc822");
            updateHeaderField(type.id(), type.toString(false, false));
            useDefault = false;
        }
    }

    if (useDefault)
    {
        type.setType("text");
        type.setSubType("plain");
        type.setCharset("us-ascii");
        updateHeaderField(type.id(), type.toString(false, false));
    }
}

/*! \internal */
uint QMailMessagePartContainerPrivate::indicativeSize() const
{
    uint size = 0;

    if (hasBody()) 
    {
        size = body().indicativeSize();
    }
    else
    {
        for (int i = 0; i < _messageParts.count(); ++i)
            size += _messageParts[i].indicativeSize();
    }

    return size;
}

void QMailMessagePartContainerPrivate::outputParts(QDataStream& out, bool includePreamble, bool includeAttachments, bool stripInternal) const
{
    static const DataString newLine('\n');
    static const DataString marker("--");

    if (_multipartType == QMailMessagePartContainer::MultipartNone)
        return;

    if (includePreamble) {
        // This is a preamble (not for conformance, to assist readibility on non-conforming renderers):
        out << DataString("This is a multipart message in Mime 1.0 format"); // No tr
        out << newLine;
    }

    for ( int i = 0; i < _messageParts.count(); i++ ) {
        out << newLine << marker << DataString(_boundary) << newLine;

        QMailMessagePart& part = const_cast<QMailMessagePart&>(_messageParts[i]);

        if (part.multipartType() != QMailMessagePartContainer::MultipartNone) {
            part.setBoundary("[}" + _boundary + "-" + QByteArray::number(i) + "}]" );
        }
        part.output(out, includeAttachments, stripInternal);
    }

    out << newLine << marker << DataString(_boundary) << marker << newLine;
}

void QMailMessagePartContainerPrivate::outputBody(QDataStream& out, bool includeAttachments) const
{
    _body.output(out, includeAttachments);
}

static QString decodedContent(const QString& id, const QByteArray& content)
{
    // TODO: Potentially, we should disallow decoding here based on the specific header field
    bool permitDecoding(true);
    //QByteArray id(fieldId(to7BitAscii(id)));
    Q_UNUSED(id)

    return (permitDecoding ? QMailMessageHeaderField::decodeContent(content) : QString(content));
}

/*!
    Returns the text of the first header field with the given \a id.
*/
QString QMailMessagePartContainerPrivate::headerFieldText( const QString &id ) const
{
    const QByteArray& content = headerField( to7BitAscii(id) );
    return decodedContent( id, content );
}

static QMailMessageContentType updateContentType(const QByteArray& existing, QMailMessagePartContainer::MultipartType multipartType, const QByteArray& boundary)
{
    // Ensure that any parameters of the existing field are preserved
    QMailMessageContentType existingType(existing);
    QList<QMailMessageHeaderField::ParameterType> parameters = existingType.parameters();

    QMailMessageContentType type(nameForMultipartType(multipartType));
    foreach (const QMailMessageHeaderField::ParameterType& param, parameters)
        type.setParameter(param.first, param.second);

    if (!boundary.isEmpty())
        type.setBoundary(boundary);

    return type;
}

void QMailMessagePartContainerPrivate::setMultipartType(QMailMessagePartContainer::MultipartType type)
{
    // TODO: Is there any value in keeping _multipartType and _boundary externally from
    // Content-type header field?

    if (_multipartType != type)
    {
        _multipartType = type;
        setDirty();

        if (_multipartType == QMailMessagePartContainer::MultipartNone) {
            removeHeaderField("Content-Type");
        } else  {
            QMailMessageContentType contentType = updateContentType(headerField("Content-Type"), _multipartType, _boundary);
            updateHeaderField("Content-Type", contentType.toString(false, false));
        }
    }
}

QByteArray QMailMessagePartContainerPrivate::boundary() const
{
    return _boundary;
}

void QMailMessagePartContainerPrivate::setBoundary(const QByteArray& text)
{
    _boundary = text;

    if (_multipartType != QMailMessagePartContainer::MultipartNone) {
        QMailMessageContentType type = updateContentType(headerField("Content-Type"), _multipartType, _boundary);
        updateHeaderField("Content-Type", type.toString(false, false));
    } else {
        QMailMessageHeaderField type("Content-Type", headerField("Content-Type"));
        type.setParameter("boundary", _boundary);
        updateHeaderField("Content-Type", type.toString(false, false));
    }
}

QMailMessageBody& QMailMessagePartContainerPrivate::body()
{
    return _body;
}

const QMailMessageBody& QMailMessagePartContainerPrivate::body() const
{
    return const_cast<QMailMessagePartContainerPrivate*>(this)->_body;
}

void QMailMessagePartContainerPrivate::setBody(const QMailMessageBody& body)
{
    _body = body;
    _hasBody = true;

    // Set the body's properties into our header
    QMailMessageContentType type(body.contentType());
    updateHeaderField(type.id(), type.toString(false, false));
    updateHeaderField("Content-Transfer-Encoding", QByteArray(nameForEncoding(body.transferEncoding())));

    setDirty();
}

bool QMailMessagePartContainerPrivate::hasBody() const
{
    return _hasBody;
}

static QByteArray plainId(const QByteArray &id)
{
    QByteArray name(id.trimmed());
    if (name.endsWith(':'))
        name.chop(1);
    return name.trimmed();
}

QByteArray QMailMessagePartContainerPrivate::headerField( const QByteArray &name ) const
{
    QList<QByteArray> result = headerFields(name, 1);
    if (result.count())
        return result[0];

    return QByteArray();
}

QList<QByteArray> QMailMessagePartContainerPrivate::headerFields( const QByteArray &name, int maximum ) const
{
    QList<QByteArray> result;

    QByteArray id(plainId(name));

    foreach (const QByteArray* field, _header.fieldList()) {
        QMailMessageHeaderField headerField(*field, QMailMessageHeaderField::UnstructuredField);
        if (insensitiveEqual(headerField.id(), id)) {
            result.append(headerField.content());
            if (maximum > 0 && result.count() == maximum)
                break;
        }
    }

    return result;
}

QList<QByteArray> QMailMessagePartContainerPrivate::headerFields() const
{
    QList<QByteArray> result;

    foreach (const QByteArray* field, _header.fieldList())
        result.append(*field);

    return result;
}

void QMailMessagePartContainerPrivate::updateHeaderField(const QByteArray &id, const QByteArray &content)
{
    _header.update(id, content);
    setDirty();

    if (insensitiveEqual(plainId(id), "Content-Type"))
    {
        // Extract the stored parts from the supplied field
        QMailMessageContentType type(content);
        _multipartType = multipartTypeForName(type.content());
        _boundary = type.boundary();
    }
}

static QByteArray encodedContent(const QByteArray& id, const QString& content)
{
    // TODO: Potentially, we should disallow encoding here based on the specific header field
    bool permitEncoding(true);
    //QByteArray name(fieldId(id));
    Q_UNUSED(id)

    return (permitEncoding ? QMailMessageHeaderField::encodeContent(content) : to7BitAscii(content));
}

void QMailMessagePartContainerPrivate::updateHeaderField(const QByteArray &id, const QString &content)
{
    updateHeaderField(id, encodedContent(id, content));
}

void QMailMessagePartContainerPrivate::appendHeaderField(const QByteArray &id, const QByteArray &content)
{
    _header.append( id, content );
    setDirty();

    if (insensitiveEqual(plainId(id), "Content-Type"))
    {
        // Extract the stored parts from the supplied field
        QMailMessageContentType type(content);
        _multipartType = multipartTypeForName(type.content());
        _boundary = type.boundary();
    }
}

void QMailMessagePartContainerPrivate::appendHeaderField(const QByteArray &id, const QString &content)
{
    appendHeaderField(id, encodedContent(id, content));
}

void QMailMessagePartContainerPrivate::removeHeaderField(const QByteArray &id)
{
    _header.remove(id);
    setDirty();

    if (insensitiveEqual(plainId(id), "Content-Type"))
    {
        // Extract the stored parts from the supplied field
        _multipartType = QMailMessagePartContainer::MultipartNone;
        _boundary = QByteArray();
    }
}

void QMailMessagePartContainerPrivate::appendPart(const QMailMessagePart &part)
{
    QList<QMailMessagePart>::iterator it = _messageParts.insert( _messageParts.end(), part );
    (*it).setPartNumber(_messageParts.count() - 1);

    setDirty();
}

void QMailMessagePartContainerPrivate::prependPart(const QMailMessagePart &part)
{
    // Increment the part numbers for existing parts
    QList<QMailMessagePart>::iterator it = _messageParts.begin(), end = _messageParts.end();
    for (int n = 1; it != end; ++it, ++n)
        (*it).setPartNumber(n);

    it = _messageParts.insert( _messageParts.begin(), part );
    (*it).setPartNumber(0);

    setDirty();
}

void QMailMessagePartContainerPrivate::clear()
{
    if (_messageParts.count())
    {
        _messageParts.clear();
        setDirty();
    }
}

QMailMessageContentType QMailMessagePartContainerPrivate::contentType() const
{
    return QMailMessageContentType(headerField("Content-Type"));
}

QMailMessageBody::TransferEncoding QMailMessagePartContainerPrivate::transferEncoding() const
{
    return encodingForName(headerField("Content-Transfer-Encoding"));
}

void QMailMessagePartContainerPrivate::parseMimeSinglePart(const QMailMessageHeader& partHeader, LongString body, int partNumber)
{
    // Create a part to contain this data
    QMailMessagePart part;
    part.setHeader(partHeader, this);
    part.setPartNumber(partNumber);

    QMailMessageContentType contentType(part.headerField("Content-Type"));
    QMailMessageBody::TransferEncoding encoding = encodingForName(part.headerFieldText("Content-Transfer-Encoding").toLatin1());
    if ( encoding == QMailMessageBody::NoEncoding )
        encoding = QMailMessageBody::SevenBit;

    if ( contentType.type() == "message" ) { // No tr
        // TODO: We can't currently handle these types
    }

    //load the body from the attachment path if it has an attachment
    //otherwise add the data to the part, in encoded form
    if(!part.attachmentPath().isEmpty())
        part.setBody(QMailMessageBody::fromFile(part.attachmentPath(), contentType, encoding, QMailMessageBody::RequiresEncoding));
    else 
        part.setBody(QMailMessageBody::fromLongString(body, contentType, encoding, QMailMessageBody::AlreadyEncoded));

    _messageParts.append( part );
}

void QMailMessagePartContainerPrivate::parseMimeMultipart(const QMailMessageHeader& partHeader, LongString body, int partNumber, bool insertIntoSelf)
{
    static const QByteArray newLine(QMailMessage::CRLF);
    static const QByteArray marker("--");

    QMailMessagePart part;
    QMailMessageContentType contentType;
    QByteArray boundary;
    QMailMessagePartContainerPrivate* multipartContainer = 0;

    if (insertIntoSelf) {
        // Insert the parts into ourself
        multipartContainer = this;
        contentType = QMailMessageContentType(headerField("Content-Type"));
        boundary = _boundary;
    }
    else {
        // This object already contains part(s) - use a new part to contain the parts
        multipartContainer = privatePointer(part);

        // Parse the header fields, and update the part
        part.setHeader(partHeader, this);
        part.setPartNumber(partNumber);
        contentType = QMailMessageContentType(part.headerField("Content-Type"));
        boundary = contentType.boundary();
    }

    // Separate the body into parts delimited by the boundary, and parse them individually
    QByteArray partDelimiter = marker + boundary;
    QByteArray partTerminator = newLine + partDelimiter + marker;

    int startPos = body.indexOf(partDelimiter, 0);
    if (startPos != -1)
        startPos += partDelimiter.length();

    // Subsequent delimiters include the leading newline
    partDelimiter.prepend(newLine);

    partNumber = 0;
    int endPos = body.indexOf(partTerminator, 0);
    while ((startPos != -1) && (startPos < endPos))
    {
        // Skip the boundary line
        startPos = body.indexOf(newLine, startPos);

        if ((startPos != -1) && (startPos < endPos))
        {
            // Parse the section up to the next boundary marker
            int nextPos = body.indexOf(partDelimiter, startPos);
            if (multipartContainer->parseMimePart(body.mid(startPos, (nextPos - startPos)), partNumber))
                ++partNumber;

            // Try the next part
            startPos = nextPos + partDelimiter.length();
        }
    }

    if (part.partCount() > 0) {
        _messageParts.append( part );
    }
}

bool QMailMessagePartContainerPrivate::parseMimePart(LongString body, int partNumber)
{
    static const QByteArray delimiter((QByteArray(QMailMessage::CRLF) + QMailMessage::CRLF));

    int startPos = 0;
    int endPos = body.indexOf(delimiter);

    if (startPos <= endPos) {
        // startPos is the offset of the header, endPos of the delimiter preceding the body
        QByteArray header(body.mid(startPos, endPos - startPos).toQByteArray());
        QMailMessageHeader partHeader = QMailMessageHeader( header );

        // Bypass the delimiter
        endPos += delimiter.length();
        LongString remainder = body.mid(endPos);

        // Is this a multipart part?
        QMailMessageContentType contentType(partHeader.field("Content-Type"));
        if (insensitiveEqual(contentType.type(), "multipart"))
        {
            // Parse the body as a multi-part
            parseMimeMultipart(partHeader, remainder, partNumber, false);
        }
        else
        {
            // Parse the remainder as a single part
            parseMimeSinglePart(partHeader, remainder, partNumber);
        }

        return true;
    }

    return false;
}

bool QMailMessagePartContainerPrivate::dirty() const
{
    if (_dirty)
        return true;

    foreach (const QMailMessagePart& part, _messageParts)
        if (part.impl<const QMailMessagePartContainerPrivate>()->dirty())
            return true;

    return false;
}

void QMailMessagePartContainerPrivate::setDirty(bool value, bool recursive)
{
    _dirty = value;

    if (recursive)
    {
        const QList<QMailMessagePart>::Iterator end = _messageParts.end();
        for (QList<QMailMessagePart>::Iterator it = _messageParts.begin(); it != end; ++it)
            (*it).impl<QMailMessagePartContainerPrivate>()->setDirty(value, true);
    }
}

template <typename Stream> 
void QMailMessagePartContainerPrivate::serialize(Stream &stream) const
{
    stream << _multipartType;
    stream << _messageParts;
    stream << _boundary;
    stream << _header;
    stream << _hasBody;
    if (_hasBody)
        stream << _body;
    stream << _dirty;
}

template <typename Stream> 
void QMailMessagePartContainerPrivate::deserialize(Stream &stream)
{
    stream >> _multipartType;
    stream >> _messageParts;
    stream >> _boundary;
    stream >> _header;
    stream >> _hasBody;
    if (_hasBody)
        stream >> _body;
    stream >> _dirty;
}

template class QPrivatelyImplemented<QMailMessagePartContainerPrivate>;


/*!
    \class QMailMessagePartContainer
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessagePartContainer class provides access to a collection of message parts.
    
    \ingroup messaginglibrary
   
    Message formats such as email messages conforming to 
    \l{http://www.ietf.org/rfc/rfc2822.txt} {RFC 2822} (Internet Message Format) can consist of 
    multiple independent parts, whose relationship to each other is defined by the message that 
    contains those parts.  The QMailMessagePartContainer class provides storage for these related 
    message parts, and the interface through which they are accessed.

    The multipartType() function returns a member of the MultipartType enumeration, which 
    describes the relationship of the parts in the container to each other.

    The part container can instead contain a message body element.  In this case, it cannot contain
    sub-parts, and the multipartType() function will return MultipartType::MultipartNone for the part. 
    The body element can be accessed via the body() function.

    The QMailMessagePart class is itself derived from QMailMessagePartContainer, which allows
    messages to support the nesting of part collections within other part collections.

    \sa QMailMessagePart, QMailMessage, QMailMessageBody
*/    

/*!
    \typedef QMailMessagePartContainer::ImplementationType
    \internal
*/

/*!
    \enum QMailMessagePartContainer::MultipartType

    This enumerated type is used to describe the multipart encoding of a message or message part.

    \value MultipartNone        The container does not hold parts.
    \value MultipartSigned      The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc1847.txt}{RFC 1847} "multipart/signed"
    \value MultipartEncrypted   The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc1847.txt}{RFC 1847} "multipart/encrypted"
    \value MultipartMixed       The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc2046.txt}{RFC 2046} "multipart/mixed"
    \value MultipartAlternative The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc2046.txt}{RFC 2046} "multipart/alternative"
    \value MultipartDigest      The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc2046.txt}{RFC 2046} "multipart/digest"
    \value MultipartParallel    The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc2046.txt}{RFC 2046} "multipart/parallel"
    \value MultipartRelated     The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc2387.txt}{RFC 2387} "multipart/related"
    \value MultipartFormData    The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc2388.txt}{RFC 2388} "multipart/form-data"
    \value MultipartReport      The container holds parts encoded according to \l {http://www.ietf.org/rfc/rfc3462.txt}{RFC 3462} "multipart/report"
*/

/*!
    \fn QMailMessagePartContainer::QMailMessagePartContainer(Subclass*)

    Constructs an empty part container object, in the space allocated 
    within the subclass instance at \a p.
*/
template<typename Subclass>
QMailMessagePartContainer::QMailMessagePartContainer(Subclass* p)
    : QPrivatelyImplemented<QMailMessagePartContainerPrivate>(p)
{
}

/*! 
    This should only be exposed if QMailMessageHeader is itself...
    \internal
*/
void QMailMessagePartContainer::setHeader(const QMailMessageHeader& partHeader, const QMailMessagePartContainerPrivate* parent)
{
    impl(this)->setHeader(partHeader, parent);
}

/*!
    Returns the number of attachments the message has.
*/  
uint QMailMessagePartContainer::partCount() const
{
    return impl(this)->_messageParts.count();
}

/*!
    Append \a part to the list of attachments for the message.
*/
void QMailMessagePartContainer::appendPart(const QMailMessagePart &part)
{
    impl(this)->appendPart( part );
}

/*!
    Prepend \a part to the list of attachments for the message.
*/
void QMailMessagePartContainer::prependPart(const QMailMessagePart &part)
{
    impl(this)->prependPart( part );
}

/*!
    Returns a const reference to the item at position \a pos in the list of 
    attachments for the message.

    pos must be a valid index position in the list (i.e., 0 <= i < partCount()).
*/
const QMailMessagePart& QMailMessagePartContainer::partAt(uint pos) const
{
    return impl(this)->_messageParts[pos];
}

/*!
    Returns a non-const reference to the item at position \a pos in the list of 
    attachments for the message.

    pos must be a valid index position in the list (i.e., 0 <= i < partCount()).
*/
QMailMessagePart& QMailMessagePartContainer::partAt(uint pos)
{
    return impl(this)->_messageParts[pos];
}

/*!
    Clears the list of attachments associated with the message.
*/
void QMailMessagePartContainer::clearParts()
{
    impl(this)->clear();
}

/*!
    Returns the type of multipart relationship shared by the parts contained within this container, or
    \l {QMailMessagePartContainer::MultipartNone}{MultipartNone} if the content is not a multipart message.
*/  
QMailMessagePartContainer::MultipartType QMailMessagePartContainer::multipartType() const
{
    return impl(this)->_multipartType;
}

/*!
    Sets the multipart state of the message to \a type.
*/
void QMailMessagePartContainer::setMultipartType(QMailMessagePartContainer::MultipartType type)
{
    impl(this)->setMultipartType(type);
}

/*!
    Returns the boundary text used to delimit the container's parts when encoded in RFC 2822 form.
*/
QByteArray QMailMessagePartContainer::boundary() const
{
    return impl(this)->boundary();
}

/*!
    Sets the boundary text used to delimit the container's parts when encoded in RFC 2822 form to \a text.
*/
void QMailMessagePartContainer::setBoundary(const QByteArray& text)
{
    impl(this)->setBoundary(text);
}

/*!
    Sets the part to contain the body element \a body.
*/
void QMailMessagePartContainer::setBody(const QMailMessageBody& body)
{
    impl(this)->setBody(body);
}

/*!
    Returns the body element contained by the part.
*/
QMailMessageBody QMailMessagePartContainer::body() const
{
    return impl(this)->body();
}

/*!
    Returns true if the part contains a body element; otherwise returns false.
*/
bool QMailMessagePartContainer::hasBody() const
{
    return impl(this)->hasBody();
}

/*!
    Returns the content type of this part.  Where hasBody() is true, the type of the
    contained body element is returned; otherwise a content type matching the 
    multipartType() for this part is returned.

    \sa hasBody(), QMailMessageBody::contentType(), multipartType()
*/
QMailMessageContentType QMailMessagePartContainer::contentType() const
{
    return impl(this)->contentType();
}

/*!
    Returns the transfer encoding type of this part.  Where hasBody() is true, the
    transfer encoding type of the contained body element is returned; otherwise, the
    transfer encoding type specified by the 'Content-Transfer-Encoding' field of the 
    header for this part is returned.

    \sa hasBody(), QMailMessageBody::transferEncoding()
*/
QMailMessageBody::TransferEncoding QMailMessagePartContainer::transferEncoding() const
{
    return impl(this)->transferEncoding();
}

/*!
    Returns the text of the first header field with the given \a id.
*/
QString QMailMessagePartContainer::headerFieldText( const QString &id ) const
{
    return impl(this)->headerFieldText(id);
}

/*!
    Returns an object containing the value of the first header field with the given \a id.
    If \a fieldType is QMailMessageHeaderField::StructuredField, then the field content 
    will be parsed assuming a format equivalent to that used for the RFC 2045 'Content-Type' 
    and RFC 2183 'Content-Disposition' header fields.
*/
QMailMessageHeaderField QMailMessagePartContainer::headerField( const QString &id, QMailMessageHeaderField::FieldType fieldType ) const
{
    QByteArray plainId( to7BitAscii(id) );
    const QByteArray& content = impl(this)->headerField( plainId );
    if ( !content.isEmpty() )
        return QMailMessageHeaderField( plainId, content, fieldType );

    return QMailMessageHeaderField();
}

/*!
    Returns a list containing the text of each header field with the given \a id.
*/
QStringList QMailMessagePartContainer::headerFieldsText( const QString &id ) const
{
    QStringList result;

    foreach (const QByteArray& item, impl(this)->headerFields( to7BitAscii(id) ))
        result.append(decodedContent( id, item ));

    return result;
}

/*!
    Returns a list of objects containing the value of each header field with the given \a id.
    If \a fieldType is QMailMessageHeaderField::StructuredField, then the field content will 
    be parsed assuming a format equivalent to that used for the RFC 2045 'Content-Type' and 
    RFC 2183 'Content-Disposition' header fields.
*/
QList<QMailMessageHeaderField> QMailMessagePartContainer::headerFields( const QString &id, QMailMessageHeaderField::FieldType fieldType ) const
{
    QList<QMailMessageHeaderField> result;

    QByteArray plainId( to7BitAscii(id) );
    foreach (const QByteArray& content, impl(this)->headerFields( plainId ))
        result.append( QMailMessageHeaderField( plainId, content, fieldType ) );

    return result;
}

/*!
    Returns a list of objects containing the value of each header field contained by the part.
    Header field objects returned by this function are not 'structured'.
*/
QList<QMailMessageHeaderField> QMailMessagePartContainer::headerFields() const
{
    QList<QMailMessageHeaderField> result;

    foreach (const QByteArray& content, impl(this)->headerFields())
        result.append( QMailMessageHeaderField( content, QMailMessageHeaderField::UnstructuredField) );

    return result;
}

/*!
    Sets the value of the first header field with identity \a id to \a value if it already 
    exists; otherwise adds the header with the supplied id and value.  If \a value is of 
    the form "<id>:<content>", then only the part after the semi-colon is processed.

    RFC 2822 encoding requires header fields to be transmitted in ASCII characters.  
    If \a value contains non-ASCII characters, it will be encoded to ASCII via the 
    QMailMessageHeaderField::encodeContent() function; depending on the specific header 
    field this may result in illegal content.  Where possible, clients should encode 
    non-ASCII data prior to calling setHeaderField.

    \sa QMailMessageHeaderField
*/
void QMailMessagePartContainer::setHeaderField( const QString& id, const QString& value )
{
    QByteArray plainId( to7BitAscii(id) );

    int index = value.indexOf(':');
    if (index != -1 ) {
        // Is the header field id replicated in the value?
        QString prefix(value.left(index));
        if ( insensitiveEqual( to7BitAscii(prefix.trimmed()), plainId.trimmed() ) ) {
            impl(this)->updateHeaderField( plainId, value.mid(index + 1) );
            return;
        }
    }

    impl(this)->updateHeaderField( plainId, value );
}

/*!
    Sets the first header field with identity matching \a field to have the content of
    \a field.
*/
void QMailMessagePartContainer::setHeaderField( const QMailMessageHeaderField& field )
{
    impl(this)->updateHeaderField( field.id(), field.toString(false, false) );
}

/*!
    Appends a new header field with id \a id and value \a value to the existing
    list of header fields.  Any existing header fields with the same id are not modified.
    If \a value is of the form "<id>:<content>", then only the part after the 
    semi-colon is processed.

    RFC 2822 encoding requires header fields to be transmitted in ASCII characters.  
    If \a value contains non-ASCII characters, it will be encoded to ASCII via the 
    QMailMessageHeaderField::encodeContent() function; depending on the specific header 
    field this may result in illegal content.  Where possible, clients should encode 
    non-ASCII data prior to calling appendHeaderField.

    \sa QMailMessageHeaderField
*/
void QMailMessagePartContainer::appendHeaderField( const QString& id, const QString& value )
{
    QByteArray plainId( to7BitAscii(id) );

    int index = value.indexOf(':');
    if (index != -1 ) {
        // Is the header field id replicated in the value?
        QString prefix(value.left(index));
        if ( insensitiveEqual( to7BitAscii(prefix.trimmed()), plainId.trimmed() ) ) {
            impl(this)->appendHeaderField( plainId, value.mid(index + 1) );
            return;
        }
    }

    impl(this)->appendHeaderField( plainId, value );
}

/*!
    Appends a new header field with the properties of \a field.  Any existing header 
    fields with the same id are not modified.
*/
void QMailMessagePartContainer::appendHeaderField( const QMailMessageHeaderField& field )
{
    impl(this)->appendHeaderField( field.id(), field.toString(false, false) );
}

/*!
    Removes all existing header fields with id equal to \a id.
*/
void QMailMessagePartContainer::removeHeaderField( const QString& id )
{
    impl(this)->removeHeaderField( to7BitAscii(id) );
}

/*! \internal */
uint QMailMessagePartContainer::indicativeSize() const
{
    return impl(this)->indicativeSize();
}

/*! \internal */
void QMailMessagePartContainer::outputParts(QDataStream& out, bool includePreamble, bool includeAttachments, bool stripInternal) const
{
    impl(this)->outputParts(out, includePreamble, includeAttachments, stripInternal);
}

/*! \internal */
void QMailMessagePartContainer::outputBody( QDataStream& out, bool includeAttachments ) const
{
    impl(this)->outputBody( out, includeAttachments );
}


/* QMailMessagePart */
class QMailMessagePartPrivate : public QMailMessagePartContainerPrivate
{
public:
    QMailMessagePartPrivate();

    QString attachmentPath() const;
    
    static QByteArray attachmentField();

    void output(QDataStream& out, bool includePreamble, bool includeAttachments, bool stripInternal) const;

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    friend class QMailMessagePart;

    int _partNumber;
};

QMailMessagePartPrivate::QMailMessagePartPrivate()
    : QMailMessagePartContainerPrivate(this),
      _partNumber(-1)
{
}

QString QMailMessagePartPrivate::attachmentPath() const
{
    QString path(headerFieldText(attachmentField()));

    // If the path contains any tab characters, it is because the path is too
    // long to fit on an unbroken line, and has had a folded CRLF inserted
    return path.remove('\t');
}

QByteArray QMailMessagePartPrivate::attachmentField() 
{
    static const QByteArray att(internalPrefix() + "attachment");
    return att;
}

void QMailMessagePartPrivate::output(QDataStream& out, bool includePreamble, bool includeAttachments, bool stripInternal) const
{
    static const DataString newLine('\n');

    _header.output( out, QList<QByteArray>(), stripInternal );
    out << DataString('\n');

    if ( hasBody() ) {
        QString attPath( attachmentPath() );

        bool attachmentData( includeAttachments || attPath.isEmpty() );
        if ( attachmentData && !stripInternal && !attPath.isEmpty() ) {
            // We are preserving the link to the file, so omit the data
            attachmentData = false;
        }

        outputBody( out, attachmentData );
    } else {
        outputParts( out, includePreamble, includeAttachments, stripInternal );
    }
}

QMailMessagePartContainerPrivate* QMailMessagePartContainerPrivate::privatePointer(QMailMessagePart& part)
{
    /* Nasty workaround required to access this data without detaching a copy... */
    return const_cast<QMailMessagePartPrivate*>(static_cast<const QMailMessagePartPrivate*>(part.d.constData()));
}

template <typename Stream> 
void QMailMessagePartPrivate::serialize(Stream &stream) const
{
    QMailMessagePartContainerPrivate::serialize(stream);

    stream << _partNumber;
}

template <typename Stream> 
void QMailMessagePartPrivate::deserialize(Stream &stream)
{
    QMailMessagePartContainerPrivate::deserialize(stream);

    stream >> _partNumber;
}


//===========================================================================
/*      Mail Message Part   */

/*!
    \class QMailMessagePart
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule
    \preliminary

    \brief The QMailMessagePart class provides a convenient interface for working 
    with message attachments.

    \ingroup messaginglibrary
    
    A message part inherits the properties of QMailMessagePartContainer, and can 
    therefore contain a message body or a collection of sub-parts.  
    
    A message part differs from a message proper in that a part will often have 
    properties specified by the MIME multipart specification, not relevant to 
    messages.  These include the 'name' and 'filename' parameters of the Content-Type 
    and Content-Disposition fields, and the Content-Id and Content-Location fields.

    \sa QMailMessagePartContainer
*/

/*!
    \typedef QMailMessagePart::ImplementationType
    \internal
*/

/*!
    Constructs an empty message part object.
*/
QMailMessagePart::QMailMessagePart()
    : QMailMessagePartContainer(new QMailMessagePartPrivate)
{
}

/*!
   Creates a QMailMessagePart containing an attachment of type \a disposition, from the 
   data contained in \a filename, of content type \a type and using the transfer encoding
   \a encoding.  The current status of the data is specified as \a status.

   \sa QMailMessageBody::fromFile()
*/
QMailMessagePart QMailMessagePart::fromFile(const QString& filename,
                                            const QMailMessageContentDisposition& disposition, 
                                            const QMailMessageContentType& type, 
                                            QMailMessageBody::TransferEncoding encoding, 
                                            QMailMessageBody::EncodingStatus status)
{
    QMailMessagePart part;
    part.setBody( QMailMessageBody::fromFile( filename, type, encoding, status ) );
    part.setContentDisposition( disposition );
    part.setAttachmentPath( filename );

    return part;
}

/*!
   Creates a QMailMessagePart containing an attachment of type \a disposition, from the 
   data read from \a in, of content type \a type and using the transfer encoding
   \a encoding.  The current status of the data is specified as \a status.

   \sa QMailMessageBody::fromStream()
*/
QMailMessagePart QMailMessagePart::fromStream(QDataStream& in,
                                              const QMailMessageContentDisposition& disposition, 
                                              const QMailMessageContentType& type, 
                                              QMailMessageBody::TransferEncoding encoding, 
                                              QMailMessageBody::EncodingStatus status)
{
    QMailMessagePart part;
    part.setBody( QMailMessageBody::fromStream( in, type, encoding, status ) );
    part.setContentDisposition( disposition );

    return part;
}

/*!
   Creates a QMailMessagePart containing an attachment of type \a disposition, from the 
   data contained in \a input, of content type \a type and using the transfer encoding
   \a encoding.  The current status of the data is specified as \a status.

   \sa QMailMessageBody::fromData()
*/
QMailMessagePart QMailMessagePart::fromData(const QByteArray& input,
                                            const QMailMessageContentDisposition& disposition, 
                                            const QMailMessageContentType& type, 
                                            QMailMessageBody::TransferEncoding encoding, 
                                            QMailMessageBody::EncodingStatus status)
{
    QMailMessagePart part;
    part.setBody( QMailMessageBody::fromData( input, type, encoding, status ) );
    part.setContentDisposition( disposition );

    return part;
}

/*!
   Creates a QMailMessagePart containing an attachment of type \a disposition, from the 
   data read from \a in, of content type \a type and using the transfer encoding
   \a encoding.

   \sa QMailMessageBody::fromStream()
*/
QMailMessagePart QMailMessagePart::fromStream(QTextStream& in,
                                              const QMailMessageContentDisposition& disposition, 
                                              const QMailMessageContentType& type, 
                                              QMailMessageBody::TransferEncoding encoding)
{
    QMailMessagePart part;
    part.setBody( QMailMessageBody::fromStream( in, type, encoding ) );
    part.setContentDisposition( disposition );

    return part;
}

/*!
   Creates a QMailMessagePart containing an attachment of type \a disposition, from the 
   data contained in \a input, of content type \a type and using the transfer encoding
   \a encoding.

   \sa QMailMessageBody::fromData()
*/
QMailMessagePart QMailMessagePart::fromData(const QString& input,
                                            const QMailMessageContentDisposition& disposition, 
                                            const QMailMessageContentType& type, 
                                            QMailMessageBody::TransferEncoding encoding)
{
    QMailMessagePart part;
    part.setBody( QMailMessageBody::fromData( input, type, encoding ) );
    part.setContentDisposition( disposition );

    return part;
}

/*!
    Returns the Content-Id header field for the part, if present; otherwise returns an empty string.
*/
QString QMailMessagePart::contentID() const
{
    return headerFieldText("Content-ID");
}

/*!
    Sets the Content-Id header field for the part to contain \a id.
*/
void QMailMessagePart::setContentID(const QString &id)
{
    setHeaderField("Content-ID", id);
}

/*!
    Returns the Content-Location header field for the part, if present; 
    otherwise returns an empty string.
*/
QString QMailMessagePart::contentLocation() const
{
    return headerFieldText("Content-Location");
}

/*!
    Sets the Content-Location header field for the part to contain \a location.
*/
void QMailMessagePart::setContentLocation(const QString &location)
{
    setHeaderField("Content-Location", location);
}

/*!
    Returns the Content-Description header field for the part, if present; 
    otherwise returns an empty string.
*/
QString QMailMessagePart::contentDescription() const
{
    return headerFieldText("Content-Description");
}

/*!
    Sets the Content-Description header field for the part to contain \a description.
*/
void QMailMessagePart::setContentDescription(const QString &description)
{
    setHeaderField("Content-Description", description);
}

/*!
    Returns the Content-Disposition header field for the part.
*/
QMailMessageContentDisposition QMailMessagePart::contentDisposition() const
{
    return QMailMessageContentDisposition(headerField("Content-Disposition"));
}

/*!
    Sets the Content-Disposition header field for the part to contain \a disposition.
*/
void QMailMessagePart::setContentDisposition(const QMailMessageContentDisposition &disposition)
{
    setHeaderField("Content-Disposition", disposition.toString());
}

/*!
    Returns the Content-Language header field for the part, if present; 
    otherwise returns an empty string.
*/
QString QMailMessagePart::contentLanguage() const
{
    return headerFieldText("Content-Language");
}

/*!
    Sets the number of the part within its parent part container to \a partNumber.
*/
void QMailMessagePart::setPartNumber(int partNumber)
{
    impl(this)->_partNumber = partNumber;
}

/*!
    Returns the number of the part, if it has been set; otherwise returns -1.
*/
int QMailMessagePart::partNumber() const
{
    return impl(this)->_partNumber;
}

/*!
    Sets the Content-Language header field for the part to contain \a language.
*/
void QMailMessagePart::setContentLanguage(const QString &language)
{
    setHeaderField("Content-Language", language);
}

/*!
    Returns a non-empty string to identify the part, appropriate for display.  If the part 
    'Content-Type' header field contains a 'name' parameter, that value is used. Otherwise, 
    if the part has a 'Content-Disposition' header field containing a 'filename' parameter, 
    that value is used.  Otherwise, if the part has a 'Content-ID' header field, that value 
    is used.  Finally, a usable name will be created by combining the content type of the 
    part with the part's number.

    \sa identifier()
*/
QString QMailMessagePart::displayName() const
{
    QString id(contentType().name());

    if (id.isEmpty())
        id = contentDisposition().filename();

    if (id.isEmpty())
        id = contentID();

    if (id.isEmpty()) {
        if (impl(this)->_partNumber != -1) {
            id = QString::number(impl(this)->_partNumber) + " ";
        }
        id += contentType().content();
    }

    return id;
}

/*!
    Returns a non-empty string to identify the part, appropriate for storage.  If the part 
    has a 'Content-ID' header field, that value is used.  Otherwise, if the part has a 
    'Content-Disposition' header field containing a 'filename' parameter, that value is used.
    Otherwise, if the part 'Content-Type' header field contains a 'name' parameter, that value 
    is used.  Finally, the part's number will be returned.
*/
QString QMailMessagePart::identifier() const
{
    QString id(contentID());

    if (id.isEmpty())
        id = contentDisposition().filename();

    if (id.isEmpty())
        id = contentType().name();

    if (id.isEmpty())
        id = QString::number(impl(this)->_partNumber);

    return id;
}

static QString randomString(int length)
{
    if (length <= 0) 
        return QString();

    QString str;
    str.resize( length );

    int i = 0;
    while (length--){
        int r=random() % 62;
        r+=48;
        if (r>57) r+=7;
        if (r>90) r+=6;
        str[i++] = char(r);
    }
    return str;
}

/*!
    Returns the leaf file name to which the part will be written if detached from the message.
 */

QString QMailMessagePart::detachFileName() const
{
    QString fileName(identifier());
    if (!fileName.isEmpty()) {
        // Remove any slash characters which are invalid in filenames
        QChar* first = fileName.data(), *last = first + (fileName.length() - 1);
        for ( ; last >= first; --last)
            if (*last == '/')
                fileName.remove((last - first), 1);
    }

    // If possible, create the file with a useful filename extension
    QString existing;
    int index = fileName.lastIndexOf(".");
    if (index != -1)
        existing = fileName.mid(index + 1);

    QMimeType mt(contentType().content());
    if (!mt.id().isEmpty() && !mt.extensions().isEmpty()) {
        // See if the existing extension is a known one
        if (existing.isEmpty() || !mt.extensions().contains(existing, Qt::CaseInsensitive)) {
            if (!fileName.endsWith(".")) {
                fileName.append(".");
            }
            fileName.append(mt.extensions().first());
        }
    }

    return fileName;
}

/*!
    Writes the decoded body of the part to a file under the directory specified by \a path.
    A 'qtopiamail' directory element will be included in the full path, to avoid filename clashes.
    Returns the path of the file written on sucess, or an empty string otherwise.
 */

QString QMailMessagePart::writeBodyTo(const QString &path) const
{
    QString attachmentName(detachFileName());

    // Put the file into a qtopiamail subdirectory, to avoid potential clashes
    QString directory(path);
    if (!directory.endsWith("/"))
        directory.append("/");
    directory.append("qtopiamail");

    QString filepath = directory + "/" + attachmentName;
    while (QFile::exists(filepath))
        filepath = directory + "/" + randomString(5) + "." + attachmentName;

    if (!QDir(directory).exists()) {
        if (!QDir(path).mkdir("qtopiamail")) {
            qWarning() << "Could not create qtopiamail directory to save file " << filepath;
            return QString();
        }
    }

    if (!body().toFile(filepath, QMailMessageBody::Decoded)) {
        qWarning() << "Could not save attachment data to file " << filepath;
        return QString();
    }
    
    return filepath;
}

/*!
    Detaches a file attachment QMailMessagePart, decoding and saving the file part into 
    the directory specified by \a path, and recording the attachment path in the part. 
    The name of the file is taken from the part. If another file already exists in the path
    a new unique name of the format <filename>.<random chars> is saved.
    Returns \c true on success or \c false otherwise.
 */

bool QMailMessagePart::detachAttachment(const QString &path) 
{
    //check if the part has already been detached

    if (!attachmentPath().isEmpty()) {
        qWarning() << "Attachment already detached";
    } else {
        QString filepath(writeBodyTo(path));
        if (!filepath.isEmpty()) {
            //append the detachment header
            setAttachmentPath(filepath);
            return true;
        }
    }
    
    return false;
}

/*!
    Returns the path to a QMailMessagePart attachment file.
    Returns an empty path for parts that are not attachments or have not been detached.
*/

QString QMailMessagePart::attachmentPath() const
{
    return impl(this)->attachmentPath();
}

/*! internal */
void QMailMessagePart::setAttachmentPath(const QString& path)
{
    // Quote the path, in case it contains illegal characters
    QString quoted(path);
    quoted.prepend('"').append('"');

    if(!attachmentPath().isEmpty())
        setHeaderField(QMailMessagePartPrivate::attachmentField(),path);
    else 
        appendHeaderField(QMailMessagePartPrivate::attachmentField(),path);
}

/*! \internal */
void QMailMessagePart::output(QDataStream& out, bool includeAttachments, bool stripInternal) const
{
    return impl(this)->output(out, false, includeAttachments, stripInternal);
}

/*! 
    \fn QMailMessagePart::serialize(Stream&) const
    \internal 
*/
template <typename Stream> 
void QMailMessagePart::serialize(Stream &stream) const
{
    impl(this)->serialize(stream);
}

/*! 
    \fn QMailMessagePart::deserialize(Stream&)
    \internal 
*/
template <typename Stream> 
void QMailMessagePart::deserialize(Stream &stream)
{
    impl(this)->deserialize(stream);
}


static quint64 incomingFlag = 0;
static quint64 outgoingFlag = 0;
static quint64 sentFlag = 0;
static quint64 repliedFlag = 0;
static quint64 repliedAllFlag = 0;
static quint64 forwardedFlag = 0;
static quint64 downloadedFlag = 0;
static quint64 readFlag = 0;
static quint64 removedFlag = 0;
static quint64 readElsewhereFlag = 0;
static quint64 unloadedDataFlag = 0;
static quint64 newFlag = 0;
static quint64 allFlags = 0;

/*  QMailMessageMetaData */
class QMailMessageMetaDataPrivate : public QPrivateImplementationBase
{
public:
    QMailMessageMetaDataPrivate();

    static void initializeFlags();

    void setMessageType(QMailMessage::MessageType type);
    void setParentFolderId(const QMailFolderId& id);
    void setPreviousParentFolderId(const QMailFolderId& id);
    void setId(const QMailMessageId& id);
    void setStatus(quint64 status);
    void setFromAccount(const QString &account);
    void setParentAccountId(const QMailAccountId& id);
    void setFromMailbox(const QString &mailbox);
    void setServerUid(const QString &server);
    void setSize(uint size);
    void setContent(QMailMessage::ContentType type);

    void setSubject(const QString& s);
    void setDate(const QMailTimeStamp& timeStamp);
    void setFrom(const QString& s);
    void setTo(const QString& s);

    uint indicativeSize() const;

    bool dataModified() const;
    void committed();

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

    QMailMessage::MessageType _messageType;
    quint64 _status;
    QMailMessage::ContentType _contentType;

    QString _fromAccount;
    QMailAccountId _parentAccountId;
    QString _fromMailbox;
    QString _serverUid;
    uint _size;
    QMailMessageId _id;
    QMailFolderId _parentFolderId;
    QMailFolderId _previousParentFolderId;

    QString _subject;
    QMailTimeStamp _date;
    QString _from;
    QString _to;

    template <typename T>
    void updateMember(T& value, const T& newValue)
    {
        if (value != newValue) {
            value = newValue;
            _dirty = true;
        }
    }

    bool _dirty;

private:
    static quint64 registerFlag(const QString &name);
};

QMailMessageMetaDataPrivate::QMailMessageMetaDataPrivate()
    : QPrivateImplementationBase(this)
{
    _messageType = QMailMessage::None;
    _status = 0;
    _size = 0;
    _contentType = QMailMessage::UnknownContent;
    _dirty = false;
}

void QMailMessageMetaDataPrivate::initializeFlags()
{
    static bool flagsInitialized = false;
    if (!flagsInitialized) {
        flagsInitialized = true;

        incomingFlag = registerFlag("Incoming");
        outgoingFlag = registerFlag("Outgoing");
        sentFlag = registerFlag("Sent");
        repliedFlag = registerFlag("Replied");
        repliedAllFlag = registerFlag("RepliedAll");
        forwardedFlag = registerFlag("Forwarded");
        downloadedFlag = registerFlag("Downloaded");
        readFlag = registerFlag("Read");
        removedFlag = registerFlag("Removed");
        readElsewhereFlag = registerFlag("ReadElsewhere");
        unloadedDataFlag = registerFlag("UnloadedData");
        newFlag = registerFlag("New");

	allFlags = (incomingFlag |
		    outgoingFlag |
		    sentFlag |
		    repliedFlag |
		    repliedAllFlag |
		    forwardedFlag |
		    downloadedFlag |
		    readFlag |
		    removedFlag |
		    readElsewhereFlag |
		    unloadedDataFlag |
		    newFlag);
    }
}

void QMailMessageMetaDataPrivate::setMessageType(QMailMessage::MessageType type)
{
    updateMember(_messageType, type);
}

void QMailMessageMetaDataPrivate::setParentFolderId(const QMailFolderId& id)
{
    updateMember(_parentFolderId, id);
}

void QMailMessageMetaDataPrivate::setPreviousParentFolderId(const QMailFolderId& id)
{
    updateMember(_previousParentFolderId, id);
}

void QMailMessageMetaDataPrivate::setId(const QMailMessageId& id)
{
    updateMember(_id, id);
}

void QMailMessageMetaDataPrivate::setStatus(quint64 newStatus)
{
    updateMember(_status, newStatus);
}

void QMailMessageMetaDataPrivate::setFromAccount(const QString &account)
{
    updateMember(_fromAccount, account);
}

void QMailMessageMetaDataPrivate::setParentAccountId(const QMailAccountId& id)
{
    updateMember(_parentAccountId, id);
}

void QMailMessageMetaDataPrivate::setFromMailbox(const QString &mailbox)
{
    updateMember(_fromMailbox, mailbox);
}

void QMailMessageMetaDataPrivate::setServerUid(const QString &uid)
{
    updateMember(_serverUid, uid);
}

void QMailMessageMetaDataPrivate::setSize(uint size)
{
    updateMember(_size, size);
}

void QMailMessageMetaDataPrivate::setContent(QMailMessage::ContentType type)
{
    updateMember(_contentType, type);
}

void QMailMessageMetaDataPrivate::setSubject(const QString& s)
{
    updateMember(_subject, s);
}

void QMailMessageMetaDataPrivate::setDate(const QMailTimeStamp& timeStamp)
{
    updateMember(_date, timeStamp);
}

void QMailMessageMetaDataPrivate::setFrom(const QString& s)
{
    updateMember(_from, s);
} 

void QMailMessageMetaDataPrivate::setTo(const QString& s)
{
    updateMember(_to, s);
}

uint QMailMessageMetaDataPrivate::indicativeSize() const
{
    uint size = (_size / QMailMessageBodyPrivate::IndicativeSizeUnit);

    // Count the message header as one size unit
    return (size + 1);
}

bool QMailMessageMetaDataPrivate::dataModified() const
{
    return _dirty;
}

void QMailMessageMetaDataPrivate::committed()
{
    _dirty = false;
}

quint64 QMailMessageMetaDataPrivate::registerFlag(const QString &name)
{
    if (!QMailStore::instance()->registerMessageStatusFlag(name)) {
        qLog(Messaging) << "Unable to register message status flag:" << name << "!";
    }

    return QMailMessage::statusMask(name);
}

template <typename Stream> 
void QMailMessageMetaDataPrivate::serialize(Stream &stream) const
{
    stream << _messageType;
    stream << _status;
    stream << _contentType;
    stream << _fromAccount;
    stream << _parentAccountId;
    stream << _fromMailbox;
    stream << _serverUid;
    stream << _size;
    stream << _id;
    stream << _parentFolderId;
    stream << _previousParentFolderId;
    stream << _subject;
    stream << _date.toString();
    stream << _from;
    stream << _to;
    stream << _dirty;
}

template <typename Stream> 
void QMailMessageMetaDataPrivate::deserialize(Stream &stream)
{
    QString timeStamp;

    stream >> _messageType;
    stream >> _status;
    stream >> _contentType;
    stream >> _fromAccount;
    stream >> _parentAccountId;
    stream >> _fromMailbox;
    stream >> _serverUid;
    stream >> _size;
    stream >> _id;
    stream >> _parentFolderId;
    stream >> _previousParentFolderId;
    stream >> _subject;
    stream >> timeStamp;
    stream >> _from;
    stream >> _to;
    stream >> _dirty;

    _date = QMailTimeStamp(timeStamp);
}

template class QPrivatelyImplemented<QMailMessageMetaDataPrivate>;


/*!
    \class QMailMessageMetaData
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessageMetaData class provides information about a message stored by Qtopia.
    
    \ingroup messaginglibrary
   
    The QMailMessageMetaData class provides information about messages stored in the Qt Extended system as QMailMessage objects.  The meta data is more compact and more easily accessed and
    manipulated than the content of the message itself.  Many messaging-related tasks can 
    be accomplished by manipulating the message meta data, such as listing, filtering, and 
    searching through sets of messages.

    QMailMessageMetaData objects can be created as needed, specifying the identifier of
    the message whose meta data is required.  The meta data of a message can be located by 
    specifying the QMailMessageId identifier directly, or by specifying the account and server UID 
    pair needed to locate the message.

    The content of the message described by the meta data object can be accessed by creating 
    a QMailMessage object specifying the identifier returned by QMailMessageMetaData::id().

    \sa QMailStore, QMailMessageId
*/    
    
/*!
    \typedef QMailMessageMetaData::ImplementationType
    \internal
*/

/*!
    \enum QMailMessageMetaData::MessageType

    This enum type is used to describe the type of a message.

    \value Mms       The message is an MMS.
    \value Sms       The message is an SMS.
    \value Email     The message is an Email.
    \value Instant   The message is an instant message.
    \value System    The message is a system report.
    \value None      Indicates no message type.
    \value AnyType   Indicates any type of message.
*/

/*!
    \enum QMailMessageMetaData::ContentType

    This enum type is used to describe the type of data contained within a message.

    \value UnknownContent The content of the message has not been specified.
    \value NoContent The message does not contain content and is completely described by its meta data.
    \value PlainTextContent Plain text content.
    \value RichTextContent Text content described via QTextBrowser rich text markup.
    \value HtmlContent Content marked up via HyperText Markup Language.
    \value ImageContent Image content.
    \value AudioContent Audio content.
    \value VideoContent Video content.
    \value MultipartContent Content consisting of multiple individual parts related according to RFC 2046.
    \value SmilContent Dynamic content described via Synchronized Multimedia Integration Language.
    \value VoicemailContent Content that should be presented as a recorded audio message from a contact.
    \value VideomailContent Content that should be presented as a recorded video message from a contact.
    \value VCardContent A contact description, as defined by RFC 2425.
    \value VCalendarContent A scheduling element description as defined by the vCalendar 1.0 specification.
    \value ICalendarContent A scheduling element description as defined by RFC 2445.
    \value UserContent The first value that can be used for application-specific purposes.
*/

/*!
    \variable QMailMessageMetaData::Incoming

    The status mask needed for testing the value of the registered status flag named 
    \c "Incoming" against the result of QMailMessage::status().

    This flag indicates that the message has been sent from an external source to an
    account whose messages are retrieved to Qt Extended.
*/

/*!
    \variable QMailMessageMetaData::Outgoing

    The status mask needed for testing the value of the registered status flag named 
    \c "Outgoing" against the result of QMailMessage::status().

    This flag indicates that the message originates within Qt Extended, for transmission 
    to an external message sink.
*/

/*!
    \variable QMailMessageMetaData::Sent

    The status mask needed for testing the value of the registered status flag named 
    \c "Sent" against the result of QMailMessage::status().

    This flag indicates that the message has been delivered to an external message sink.
*/

/*!
    \variable QMailMessageMetaData::Replied

    The status mask needed for testing the value of the registered status flag named 
    \c "Replied" against the result of QMailMessage::status().

    This flag indicates that a message replying to the source of this message has been 
    created, in response to this message.
*/

/*!
    \variable QMailMessageMetaData::RepliedAll

    The status mask needed for testing the value of the registered status flag named 
    \c "RepliedAll" against the result of QMailMessage::status().

    This flag indicates that a message replying to the source of this message and all 
    its recipients, has been created in response to this message.
*/

/*!
    \variable QMailMessageMetaData::Forwarded

    The status mask needed for testing the value of the registered status flag named 
    \c "Forwarded" against the result of QMailMessage::status().

    This flag indicates that a message forwarding the content of this message has been created.
*/

/*!
    \variable QMailMessageMetaData::Downloaded

    The status mask needed for testing the value of the registered status flag named 
    \c "Downloaded" against the result of QMailMessage::status().

    This flag indicates that the entire content of the message has been retrieved from the originating server.
*/

/*!
    \variable QMailMessageMetaData::Read

    The status mask needed for testing the value of the registered status flag named 
    \c "Read" against the result of QMailMessage::status().

    This flag indicates that the content of this message has been displayed to the user.
*/

/*!
    \variable QMailMessageMetaData::Removed

    The status mask needed for testing the value of the registered status flag named 
    \c "Removed" against the result of QMailMessage::status().

    This flag indicates that the message has been deleted from or moved on the originating server.
*/

/*!
    \variable QMailMessageMetaData::ReadElsewhere

    The status mask needed for testing the value of the registered status flag named 
    \c "ReadElsewhere" against the result of QMailMessage::status().

    This flag indicates that the content of this message has been reported as having
    been displayed to the user by some other client.
*/

/*!
    \variable QMailMessageMetaData::UnloadedData

    The status mask needed for testing the value of the registered status flag named 
    \c "UnloadedData" against the result of QMailMessage::status().

    This flag indicates that the meta data of the message is not loaded in entirety.
*/

/*!
    \variable QMailMessageMetaData::New

    The status mask needed for testing the value of the registered status flag named 
    \c "New" against the result of QMailMessage::status().

    This flag indicates that the meta data of the message has not yet been displayed to the user.
*/

const quint64 &QMailMessageMetaData::Incoming = incomingFlag;
const quint64 &QMailMessageMetaData::Outgoing = outgoingFlag;
const quint64 &QMailMessageMetaData::Sent = sentFlag;
const quint64 &QMailMessageMetaData::Replied = repliedFlag;
const quint64 &QMailMessageMetaData::RepliedAll = repliedAllFlag;
const quint64 &QMailMessageMetaData::Forwarded = forwardedFlag;
const quint64 &QMailMessageMetaData::Downloaded = downloadedFlag;
const quint64 &QMailMessageMetaData::Read = readFlag;
const quint64 &QMailMessageMetaData::Removed = removedFlag;
const quint64 &QMailMessageMetaData::ReadElsewhere = readElsewhereFlag;
const quint64 &QMailMessageMetaData::UnloadedData = unloadedDataFlag;
const quint64 &QMailMessageMetaData::New = newFlag;
const quint64 &QMailMessageMetaData::All = allFlags;

/*!
    Constructs an empty message meta data object.
*/
QMailMessageMetaData::QMailMessageMetaData()
    : QPrivatelyImplemented<QMailMessageMetaDataPrivate>(new QMailMessageMetaDataPrivate())
{
}

/*!
    Constructs a message meta data object from data stored in the message store with QMailMessageId \a id.
*/
QMailMessageMetaData::QMailMessageMetaData(const QMailMessageId& id)
    : QPrivatelyImplemented<QMailMessageMetaDataPrivate>(0)
{
    *this = QMailStore::instance()->messageMetaData(id);
}

/*!
    Constructs a message meta data object from data stored in the message store with the unique 
    identifier \a uid from the account with id \a accountId.
*/
QMailMessageMetaData::QMailMessageMetaData(const QString& uid, const QMailAccountId& accountId)
    : QPrivatelyImplemented<QMailMessageMetaDataPrivate>(0)
{
    *this = QMailStore::instance()->messageMetaData(uid, accountId);
}

/*!
    Sets the MessageType of the message to \a type.

    \sa messageType()
*/
void QMailMessageMetaData::setMessageType(QMailMessageMetaData::MessageType type)
{
    switch (type) {
        case QMailMessage::Mms:
        case QMailMessage::Sms:
        case QMailMessage::Email:
        case QMailMessage::Instant:
        case QMailMessage::System:
            break;
        default:
            qWarning() << "QMailMessageMetaData::setMessageType:" << type;
            return;
    }

    impl(this)->setMessageType(type);
}

/*!
    Returns the MessageType of the message.

    \sa setMessageType()
*/
QMailMessageMetaData::MessageType QMailMessageMetaData::messageType() const
{
    return impl(this)->_messageType;
}

/*!
    Return the QMailFolderId of the folder that contains the message.
*/
QMailFolderId QMailMessageMetaData::parentFolderId() const
{
    return impl(this)->_parentFolderId;
}

/*!
    Sets the QMailFolderId of the folder that contains the message to \a id.
*/
void QMailMessageMetaData::setParentFolderId(const QMailFolderId &id)
{
    impl(this)->setParentFolderId(id);
}

/*!
    Returns the Qt Extended unique QMailMessageId of the message.
*/
QMailMessageId QMailMessageMetaData::id() const
{
    return impl(this)->_id;
}

/*!
    Sets the QMailMessageId of the message to \a id.
    \a id should be different for each message known to Qtopia.
*/
void QMailMessageMetaData::setId(const QMailMessageId &id)
{
    impl(this)->setId(id);
}

/*!
    Returns the originating address of the message.
*/
QMailAddress QMailMessageMetaData::from() const
{
    return QMailAddress(impl(this)->_from);
}

/*!
    Sets the from address, that is the originating address of the message to \a from.
*/
void QMailMessageMetaData::setFrom(const QMailAddress &from)
{
    impl(this)->setFrom(from.toString());
}

/*!
    Returns the subject of the message, if present; otherwise returns an empty string.
*/
QString QMailMessageMetaData::subject() const
{
    return impl(this)->_subject;
}

/*!
    Sets the subject of the message to \a subject.
*/
void QMailMessageMetaData::setSubject(const QString &subject)
{
    impl(this)->setSubject(subject);
}


/*!
    Returns the timestamp contained in the origination date header field of the message, if present; 
    otherwise returns an empty string.
*/
QMailTimeStamp QMailMessageMetaData::date() const
{
    return QMailTimeStamp(impl(this)->_date);
}

/*!
    Sets the origination date header field specifying the timestamp of the message to \a timeStamp.
*/
void QMailMessageMetaData::setDate(const QMailTimeStamp &timeStamp)
{
    impl(this)->setDate(timeStamp);
}

/*! 
    Returns the list of primary recipients for the message.

    \sa QMailAddress
*/
QList<QMailAddress> QMailMessageMetaData::to() const
{
    return QMailAddress::fromStringList(impl(this)->_to);
}

/*! 
    Sets the list of primary recipients for the message to \a toList.
*/
void QMailMessageMetaData::setTo(const QList<QMailAddress>& toList)
{
    impl(this)->setTo(QMailAddress::toStringList(toList).join(","));
}

/*! 
    Sets the list of primary recipients for the message to contain \a address.
*/
void QMailMessageMetaData::setTo(const QMailAddress& address)
{
    setTo(QList<QMailAddress>() << address);
}

/*!
    Returns the status value for the message.

    \sa setStatus(), statusMask()
*/  
quint64 QMailMessageMetaData::status() const
{
    return impl(this)->_status;
}

/*!
    Sets the status value for the message to \a newStatus.

    \sa status(), statusMask()
*/
void QMailMessageMetaData::setStatus(quint64 newStatus)
{
    impl(this)->setStatus(newStatus);
}

/*!
    Sets the status flags indicated in \a mask to \a set.

    \sa status(), statusMask()
*/
void QMailMessageMetaData::setStatus(quint64 mask, bool set)
{
    quint64 newStatus = impl(this)->_status;

    if (set)
        newStatus |= mask;
    else
        newStatus &= ~mask;
    impl(this)->setStatus(newStatus);
}

/*!
    Returns the name of the originating account for the message.
*/  
QString QMailMessageMetaData::fromAccount() const
{
    QMailAccount account = QMailStore::instance()->account(impl(this)->_parentAccountId);
    return account.accountName();
}

/*!
    Sets the name of the originating account for the message to \a account.
*/  
void QMailMessageMetaData::setFromAccount(const QString &account)
{
    QMailAccountKey key(QMailAccountKey::Name,account);
    QMailAccountIdList ids = QMailStore::instance()->queryAccounts(key);
    if(ids.count() != 1) {
        qWarning() << "Unable to set message parent account with name " << account;
        return;
    }

    impl(this)->setParentAccountId(ids.first());
}

/*!
    Returns the id of the originating account for the message.
*/  

QMailAccountId QMailMessageMetaData::parentAccountId() const
{
    return impl(this)->_parentAccountId;
}

/*!
    Sets the id of the originating account for the message to \a id.
*/ 
void QMailMessageMetaData::setParentAccountId(const QMailAccountId& id)
{
    impl(this)->setParentAccountId(id);
}

/*!
    Returns the name of the originating mailbox for the message.
*/  
QString QMailMessageMetaData::fromMailbox() const
{
    return impl(this)->_fromMailbox;
}

/*!
    Sets the name of the originating mailbox for the message to \a mailBox.
*/  
void QMailMessageMetaData::setFromMailbox(const QString &mailBox)
{
    impl(this)->setFromMailbox(mailBox);
}

/*!
    Returns the identifier for the message on the originating server.
*/  
QString QMailMessageMetaData::serverUid() const
{
    return impl(this)->_serverUid;
}

/*!
    Sets the originating server identifier for the message to \a server.
    The identifier specified should be unique.
*/  
void QMailMessageMetaData::setServerUid(const QString &server)
{
    impl(this)->setServerUid(server);
}

/*!
    Returns the complete size of the message as indicated on the originating server.
*/  
uint QMailMessageMetaData::size() const
{
    return impl(this)->_size;
}

/*!
    Sets the complete size of the message as found on the server to \a size.
*/  
void QMailMessageMetaData::setSize(uint size)
{
    impl(this)->setSize(size);
}

/*!
    Returns an indication of the size of the message.  This measure should be used
    only in comparing the relative size of messages with respect to transmission.
*/  
uint QMailMessageMetaData::indicativeSize() const
{
    return impl(this)->indicativeSize();
}

/*!
    Returns the type of content contained within the message.
*/  
QMailMessage::ContentType QMailMessageMetaData::content() const
{
    return impl(this)->_contentType;
}

/*!
    \fn QMailMessageMetaData::setContent(QMailMessageMetaData::ContentType)

    Sets the type of content contained within the message to \a type.  
    It is the caller's responsibility to ensure that this value matches the actual content.
*/  
void QMailMessageMetaData::setContent(QMailMessage::ContentType type)
{
    impl(this)->setContent(type);
}

/*!
    Return the QMailFolderId of the folder that contained the message before it was 
    moved into the current parent folder.
*/
QMailFolderId QMailMessageMetaData::previousParentFolderId() const
{
    return impl(this)->_previousParentFolderId;
}

/*!
    Sets the QMailFolderId of the folder that contained the message before it was 
    moved into the current parent folder to \a id.
*/
void QMailMessageMetaData::setPreviousParentFolderId(const QMailFolderId &id)
{
    impl(this)->setPreviousParentFolderId(id);
}

/*! \internal */
bool QMailMessageMetaData::dataModified() const
{
    return impl(this)->dataModified();
}

/*! \internal */
void QMailMessageMetaData::committed()
{
    impl(this)->committed();
}

/*!
    Returns the status bitmask needed to test the result of QMailMessageMetaData::status() 
    against the QMailMessageMetaData status flag registered with the identifier \a flagName.

    \sa status(), QMailStore::messageStatusMask()
*/
quint64 QMailMessageMetaData::statusMask(const QString &flagName)
{
    return QMailStore::instance()->messageStatusMask(flagName);
}

/*! \internal */
void QMailMessageMetaData::initStore()
{
    QMailMessageMetaDataPrivate::initializeFlags();
}

/*! 
    \fn QMailMessageMetaData::serialize(Stream&) const
    \internal 
*/
template <typename Stream> 
void QMailMessageMetaData::serialize(Stream &stream) const
{
    impl(this)->serialize(stream);
}

/*! 
    \fn QMailMessageMetaData::deserialize(Stream&)
    \internal 
*/
template <typename Stream> 
void QMailMessageMetaData::deserialize(Stream &stream)
{
    impl(this)->deserialize(stream);
}


/*  QMailMessage */
class QMailMessagePrivate : public QMailMessagePartContainerPrivate
{
public:
    QMailMessagePrivate();

    void fromRfc2822(const LongString &ls);
    void toRfc2822(QDataStream& out, QMailMessage::EncodingFormat format, quint64 messageStatus) const;

    void setMessageType(QMailMessage::MessageType type);

    void setSubject(const QString& s);
    void setDate(const QMailTimeStamp& timeStamp);
    void setFrom(const QString& from);
    void setReplyTo(const QString& s);
    void setInReplyTo(const QString& s);

    void setTo(const QString& s);
    void setCc(const QString& s);
    void setBcc(const QString& s);

    bool hasRecipients() const;

    uint indicativeSize() const;

    bool contentModified() const;
    void committed();

    template <typename Stream> void serialize(Stream &stream) const;
    template <typename Stream> void deserialize(Stream &stream);

private:
    void outputHeaders(QDataStream& out, bool addTimeStamp, bool addContentHeaders, bool includeBcc, bool stripInternal) const;

    LongString _rawMessageBody;
};

QMailMessagePrivate::QMailMessagePrivate()
    : QMailMessagePartContainerPrivate(this)
{
}

void QMailMessagePrivate::fromRfc2822(const LongString &ls)
{
    _messageParts.clear();
    _rawMessageBody = ls;

    if (ls.length()) {
        QMailMessageContentType contentType(headerField("Content-Type"));

        // Is this a simple mail or a multi-part collection?
        QByteArray mimeVersion = headerField("MIME-Version");
        QByteArray minimalVersion = QMailMessageHeaderField::removeWhitespace(QMailMessageHeaderField::removeComments(mimeVersion));
        if (!mimeVersion.isEmpty() && (minimalVersion != "1.0")) {
            qWarning() << "Unknown MIME-Version:" << mimeVersion;
        } else if (_multipartType != QMailMessagePartContainer::MultipartNone) {
            parseMimeMultipart(_header, _rawMessageBody, 0, true);
        } else {
            QByteArray bodyData;

            // Remove the pop-style terminator if present
            const QByteArray popTerminator((QByteArray(QMailMessage::CRLF) + '.' + QMailMessage::CRLF));
            if ( _rawMessageBody.indexOf(popTerminator, -popTerminator.length()) != -1)
                bodyData = _rawMessageBody.left( _rawMessageBody.length() - popTerminator.length() ).toQByteArray();
            else
                bodyData = _rawMessageBody.toQByteArray();

            // The body data is already encoded
            QDataStream in(bodyData);
            QMailMessageBody::TransferEncoding encoding = encodingForName(headerField("Content-Transfer-Encoding"));
            if ( encoding == QMailMessageBody::NoEncoding )
                encoding = QMailMessageBody::SevenBit;

            setBody( QMailMessageBody::fromStream(in, contentType, encoding, QMailMessageBody::AlreadyEncoded) );
        }
    }
}

void QMailMessagePrivate::setSubject(const QString& s)
{
    updateHeaderField( "Subject:", s );
}

void QMailMessagePrivate::setDate(const QMailTimeStamp& timeStamp)
{
    updateHeaderField( "Date:", to7BitAscii(timeStamp.toString()) );
}

void QMailMessagePrivate::setFrom(const QString& s)
{
    updateHeaderField( "From:", s );
} 

void QMailMessagePrivate::setReplyTo(const QString& s)
{
    updateHeaderField( "Reply-To:", s );
}

void QMailMessagePrivate::setInReplyTo(const QString& s)
{
    updateHeaderField( "In-Reply-To:", s );
}

void QMailMessagePrivate::setTo(const QString& s)
{
    updateHeaderField( "To:", s );
}

void QMailMessagePrivate::setBcc(const QString& s)
{
    updateHeaderField( "Bcc:", s );
}

void QMailMessagePrivate::setCc(const QString& s)
{
    updateHeaderField( "Cc:", s );
}

bool QMailMessagePrivate::hasRecipients() const
{
    if ( !headerField("To").isEmpty() )
        return true;
    if ( !headerField("Cc").isEmpty() )
        return true;
    if ( !headerField("Bcc").isEmpty() )
        return true;

    return false;
}

uint QMailMessagePrivate::indicativeSize() const
{
    uint size = QMailMessagePartContainerPrivate::indicativeSize(); 

    // Count the message header as one size unit
    return (size + 1);
}

void QMailMessagePrivate::toRfc2822(QDataStream& out, QMailMessage::EncodingFormat format, quint64 messageStatus) const
{
    bool isOutgoing = (messageStatus & (QMailMessage::Outgoing | QMailMessage::Sent));

    bool addTimeStamp = (format != QMailMessage::IdentityFormat);
    bool addContentHeaders = ((format != QMailMessage::IdentityFormat) && 
                              ((format != QMailMessage::StorageFormat) || isOutgoing || _rawMessageBody.isEmpty()));
    bool includeBcc = (format != QMailMessage::TransmissionFormat);
    bool includePreamble = (format == QMailMessage::TransmissionFormat);
    bool includeAttachments = ((format != QMailMessage::HeaderOnlyFormat) && (format != QMailMessage::StorageFormat));
    bool stripInternal = ((format == QMailMessage::TransmissionFormat) || (format == QMailMessage::IdentityFormat));

    if (_messageParts.count() && boundary().isEmpty())
        const_cast<QMailMessagePrivate*>(this)->setBoundary(uninitialisedBoundary);

    outputHeaders(out, addTimeStamp, addContentHeaders, includeBcc, stripInternal);
    out << DataString('\n');

    if (format != QMailMessage::HeaderOnlyFormat) {
        if ( hasBody() ) {
            outputBody( out, true); //not multipart so part should not be an attachment
        } else {
            outputParts( out, includePreamble, includeAttachments, stripInternal );
        }
    }
}

void QMailMessagePrivate::outputHeaders( QDataStream& out, bool addTimeStamp, bool addContentHeaders, bool includeBcc, bool stripInternal ) const
{
    QList<QByteArray> exclusions;

    if (addContentHeaders) {
        // Don't include the nominated MIME-Version if specified - we implement 1.0
        exclusions.append("MIME-Version");
    }
    if (!includeBcc) {
        exclusions.append("bcc");
    }

    _header.output( out, exclusions, stripInternal );

    if (addTimeStamp && headerField("Date").isEmpty()) {
        QString timeStamp = QMailTimeStamp( QDateTime::currentDateTime() ).toString();
        out << DataString("Date: ") << DataString(to7BitAscii(timeStamp)) << DataString('\n');
    }

    if (addContentHeaders) {
        // Output required content header fields
        out << DataString("MIME-Version: 1.0") << DataString('\n');
    }
}

bool QMailMessagePrivate::contentModified() const
{
    return dirty();
}

void QMailMessagePrivate::committed()
{
    setDirty(false, true);
}

template <typename Stream> 
void QMailMessagePrivate::serialize(Stream &stream) const
{
    QMailMessagePartContainerPrivate::serialize(stream);

    stream << _rawMessageBody;
}

template <typename Stream> 
void QMailMessagePrivate::deserialize(Stream &stream)
{
    QMailMessagePartContainerPrivate::deserialize(stream);

    stream >> _rawMessageBody;
}


//===========================================================================

/*!
    \class QMailMessage
    \inpublicgroup QtMessagingModule
    \inpublicgroup QtPimModule

    \preliminary
    \brief The QMailMessage class provides a convenient interface for working with messages.
    
    \ingroup messaginglibrary
   
    QMailMessage supports a number of types. These include telephony types 
    such as SMS and MMS, and internet email messages as defined in 
    \l{http://www.ietf.org/rfc/rfc2822.txt} {RFC 2822} (Internet Message Format), and 
    \l{http://www.ietf.org/rfc/rfc2045.txt} {RFC 2045} (Format of Internet Message Bodies) through 
    \l{http://www.ietf.org/rfc/rfc2049.txt} {RFC 2049} (Conformance Criteria and Examples).
    
    The most common way to use QMailMessage is to initialize it from raw
    data using QMailMessage::fromRfc2822().
    
    A QMailMessage can also be constructed piece by piece using functions such as 
    setMessageType(), setFrom(), setTo(), setSubject(), and setBody() or appendPart(). 
    Convenience functions such as from()/setFrom() and date()/setDate() accept and
    return wrapper types, to simplify the exchange of correctly-formatted data.
    In some cases, however, it may be simpler for clients to get and set the content 
    of header fields directly, using the headerField() and setHeaderField() functions inherited
    from QMailMessagePartContainer.
    
    Messages can be added to the QMailStore, or retrieved from the store via their QMailMessageId 
    identifier.  The QMailMessage object also provides acces to any relevant meta data
    describing the message, using the functions inherited from QMailMessageMetaData.

    A message may be serialized to a QDataStream, or returned as a QByteArray using toRfc2822().
    
    \sa QMailMessageMetaData, QMailMessagePart, QMailMessageBody, QMailStore, QMailMessageId
*/    
    
/*!
    \enum QMailMessage::EncodingFormat
    
    This enum type is used to describe the format in which a message should be serialized.

    \value HeaderOnlyFormat     Only the header portion of the message is serialized, to RFC 2822 form.
    \value StorageFormat        The message is serialized to RFC 2822 form, without attachments.
    \value TransmissionFormat   The entire message is serialized to RFC 2822 form, with additional header fields added if necessary, and 'bcc' header field omitted.
    \value IdentityFormat       The entire message is serialized to RFC 2822 form, with only Content-Type and Content-Transfer-Encoding headers added where required.
*/

/*!
    \enum QMailMessage::AttachmentsAction

    This enum type is used to describe the action that should be performed on 
    each message attachment.

    \value LinkToAttachments        Add a part to the message containing a link to the 
                                    supplied attachment. If the document is removed, the 
                                    message will no longer have access to the data.
    \value CopyAttachments          Add a part to the message containing a copy of the
                                    data in the supplied attachment. If the document is 
                                    removed, the message will still contain the data.
    \value CopyAndDeleteAttachments Add a part to the message containing a copy of the
                                    data in the supplied attachment, then delete the
                                    document from which the data was copied.
*/



const char QMailMessage::CarriageReturn = '\015';
const char QMailMessage::LineFeed = '\012';
const char* QMailMessage::CRLF = "\015\012";

/*!
    Constructs an empty message object.
*/
QMailMessage::QMailMessage()
    : QMailMessageMetaData(),
      QMailMessagePartContainer(new QMailMessagePrivate())
{
}

/*!
    Constructs a message object from data stored in the message store with QMailMessageId \a id.
*/
QMailMessage::QMailMessage(const QMailMessageId& id)
    : QMailMessageMetaData(id),
      QMailMessagePartContainer(reinterpret_cast<QMailMessagePrivate*>(0))
{
    *this = QMailStore::instance()->message(id);
}

/*!
    Constructs a message object from data stored in the message store with the unique 
    identifier \a uid from the account with id \a accountId.
*/
QMailMessage::QMailMessage(const QString& uid, const QMailAccountId& accountId)
    : QMailMessageMetaData(uid, accountId),
      QMailMessagePartContainer(reinterpret_cast<QMailMessagePrivate*>(0))
{
    *this = QMailStore::instance()->message(uid, accountId);
}

/*!
    Constructs a mail message from the RFC 2822 data contained in \a byteArray.
*/
QMailMessage QMailMessage::fromRfc2822(const QByteArray &byteArray)
{
    LongString ls(byteArray);
    return fromRfc2822(ls);
}

/*!
    Constructs a mail message from the RFC 2822 data contained in the file \a fileName.
*/
QMailMessage QMailMessage::fromRfc2822File(const QString& fileName)
{
    LongString ls(fileName);
    return fromRfc2822(ls);
}

/*! \reimp */
void QMailMessage::setHeaderField( const QString& id, const QString& value )
{
    QMailMessagePartContainer::setHeaderField(id, value);

    QByteArray duplicatedId(duplicatedData(id));
    if (!duplicatedId.isNull()) {
        updateMetaData(duplicatedId, value);
    }
}

/*! \reimp */
void QMailMessage::setHeaderField( const QMailMessageHeaderField& field )
{
    setHeaderField(field.id(), field.toString(false, false));
}

/*! \reimp */
void QMailMessage::appendHeaderField( const QString& id, const QString& value )
{
    QMailMessagePartContainer::appendHeaderField(id, value);

    QByteArray duplicatedId(duplicatedData(id));
    if (!duplicatedId.isNull()) {
        // We need to keep the value of the first item with this ID in the meta data object
        updateMetaData(duplicatedId, headerFieldText(duplicatedId));
    }
}

/*! \reimp */
void QMailMessage::appendHeaderField( const QMailMessageHeaderField& field )
{
    appendHeaderField(field.id(), field.toString(false, false));
}

/*! \reimp */
void QMailMessage::removeHeaderField( const QString& id )
{
    QMailMessagePartContainer::removeHeaderField(id);

    QByteArray duplicatedId(duplicatedData(id));
    if (!duplicatedId.isNull()) {
        updateMetaData(duplicatedId, QString());
    }
}

/*!
    Returns the message in RFC 2822 format. The encoded content will vary depending on the value of \a format.
*/
QByteArray QMailMessage::toRfc2822(EncodingFormat format) const
{
    QByteArray result;
    {
        QDataStream out(&result, QIODevice::WriteOnly);
        toRfc2822(out, format);
    }
    return result;
}

/*!
    Writes the message to the output stream \a out, in RFC 2822 format. 
    The encoded content will vary depending on the value of \a format.
*/
void QMailMessage::toRfc2822(QDataStream& out, EncodingFormat format) const
{
    partContainerImpl()->toRfc2822(out, format, status());
}

/*! \reimp */
void QMailMessage::setFrom(const QMailAddress &from)
{
    metaDataImpl()->setFrom(from.toString());
    partContainerImpl()->setFrom(from.toString());
}

/*! \reimp */
void QMailMessage::setSubject(const QString &subject)
{
    metaDataImpl()->setSubject(subject);
    partContainerImpl()->setSubject(subject);
}

/*! \reimp */
void QMailMessage::setDate(const QMailTimeStamp &timeStamp)
{
    metaDataImpl()->setDate(timeStamp);
    partContainerImpl()->setDate(timeStamp);
}

/*! \reimp */
void QMailMessage::setTo(const QList<QMailAddress>& toList)
{
    QString flattened(QMailAddress::toStringList(toList).join(","));
    metaDataImpl()->setTo(flattened);
    partContainerImpl()->setTo(flattened);
}

/*! \reimp */
void QMailMessage::setTo(const QMailAddress& address)
{
    setTo(QList<QMailAddress>() << address);
}

/*!
    Returns a list of all the cc (carbon copy) recipients specified for the message.

    \sa to(), bcc(), QMailAddress
*/  
QList<QMailAddress> QMailMessage::cc() const
{
    return QMailAddress::fromStringList(headerFieldText("Cc"));
}

/*!
    Set the list of cc (carbon copy) recipients for the message to \a ccList.

    \sa setTo(), setBcc()
*/  
void QMailMessage::setCc(const QList<QMailAddress>& ccList)
{
    partContainerImpl()->setCc(QMailAddress::toStringList(ccList).join(","));
}

/*!
    Returns a list of all the bcc (blind carbon copy) recipients specified for the message.

    \sa to(), cc(), QMailAddress
*/  
QList<QMailAddress> QMailMessage::bcc() const
{
    return QMailAddress::fromStringList(headerFieldText("Bcc"));
}

/*!
    Set the list of bcc (blind carbon copy) recipients for the message to \a bccList.

    \sa setTo(), setCc()
*/  
void QMailMessage::setBcc(const QList<QMailAddress>& bccList)
{
    partContainerImpl()->setBcc(QMailAddress::toStringList(bccList).join(","));
}

/*!
    Returns the address specified by the RFC 2822 'Reply-To' field for the message, if present.
*/  
QMailAddress QMailMessage::replyTo() const
{
    return QMailAddress(headerFieldText("Reply-To"));
}

/*!
    Sets the RFC 2822 'Reply-To' address of the message to \a address.
*/  
void QMailMessage::setReplyTo(const QMailAddress &address)
{
    partContainerImpl()->setReplyTo(address.toString());
}

/*!
    Returns the message ID specified by the RFC 2822 'In-Reply-To' field for the message, if present.
*/  
QString QMailMessage::inReplyTo() const
{
    return headerFieldText("In-Reply-To");
}

/*!
    Sets the RFC 2822 'In-Reply-To' field for the message to \a messageId.
*/
void QMailMessage::setInReplyTo(const QString &messageId)
{
    partContainerImpl()->setInReplyTo(messageId);
}

/*!
    Returns a list of all the recipients specified for the message, either as To, CC, or BCC addresses.

    \sa to(), cc(), bcc(), hasRecipients()
*/  
QList<QMailAddress> QMailMessage::recipients() const
{
    QList<QMailAddress> addresses;

    QStringList list;
    list.append( headerFieldText("To").trimmed() );
    list.append( headerFieldText("Cc").trimmed() );
    list.append( headerFieldText("Bcc").trimmed() );
    if (!list.isEmpty()) {
        list.removeAll( "" );
        list.removeAll( QString::null );
    }
    if (!list.isEmpty()) {
        addresses += QMailAddress::fromStringList( list.join(",") );
    }

    return addresses;
}

/*!
    Returns true if there are any recipients (either To, CC or BCC addresses) 
    defined for this message; otherwise returns false.
*/  
bool QMailMessage::hasRecipients() const
{
    return partContainerImpl()->hasRecipients();
}

/*! \reimp */  
uint QMailMessage::indicativeSize() const
{
    if (status() & QMailMessage::Incoming) {
        return metaDataImpl()->indicativeSize();
    } else if (status() & QMailMessage::Outgoing) {
        return partContainerImpl()->indicativeSize();
    }

    return 0;
}

// The QMMMetaData half of this object is implemented in a QMailMessageMetaDataPrivate object
/*! \internal */
QMailMessageMetaDataPrivate* QMailMessage::metaDataImpl() 
{ 
    return QMailMessageMetaData::impl<QMailMessageMetaDataPrivate>(); 
}

/*! \internal */
const QMailMessageMetaDataPrivate* QMailMessage::metaDataImpl() const 
{ 
    return QMailMessageMetaData::impl<const QMailMessageMetaDataPrivate>(); 
}

// The QMMPartContainer half of this object is implemented in a QMailMessagePrivate object
/*! \internal */
QMailMessagePrivate* QMailMessage::partContainerImpl() 
{ 
    return QMailMessagePartContainer::impl<QMailMessagePrivate>(); 
}

/*! \internal */
const QMailMessagePrivate* QMailMessage::partContainerImpl() const 
{ 
    return QMailMessagePartContainer::impl<const QMailMessagePrivate>(); 
}
    
/*! \internal */
bool QMailMessage::contentModified() const
{
    return partContainerImpl()->contentModified();
}

/*! \internal */
void QMailMessage::committed()
{
    metaDataImpl()->committed();
    partContainerImpl()->committed();
}

/*! \reimp */
void QMailMessage::setHeader(const QMailMessageHeader& partHeader, const QMailMessagePartContainerPrivate* parent)
{
    QMailMessagePartContainer::setHeader(partHeader, parent);

    // See if any of the header fields need to be propagated to the meta data object
    foreach (const QMailMessageHeaderField& field, headerFields()) {
        QByteArray duplicatedId(duplicatedData(field.id()));
        if (!duplicatedId.isNull()) {
            updateMetaData(duplicatedId, field.decodedContent());
        }
    }
}

/*! \internal */
QByteArray QMailMessage::duplicatedData(const QString& id) const
{
    // These items are duplicated in both the message content and the meta data
    QByteArray plainId( to7BitAscii(id).trimmed().toLower() );

    if ((plainId == "from") || (plainId == "to") || (plainId == "subject") || (plainId == "date"))
        return plainId;

    return QByteArray();
}

/*! \internal */
void QMailMessage::updateMetaData(const QByteArray& id, const QString& value)
{
    if (id == "from") {
        metaDataImpl()->setFrom(value);
    } else if (id == "to") {
        metaDataImpl()->setTo(value);
    } else if (id == "subject") {
        metaDataImpl()->setSubject(value);
    } else if (id == "date") {
        metaDataImpl()->setDate(QMailTimeStamp(value));
    }
}

/*! \internal */
QMailMessage QMailMessage::fromRfc2822(LongString& ls)
{
    const QByteArray terminator((QByteArray(QMailMessage::CRLF) + QMailMessage::CRLF));

    QMailMessage mail;

    int pos = ls.indexOf(terminator);
    if (pos == -1) {
        // No body? Parse entirety as header
        mail.setHeader( QMailMessageHeader( ls.toQByteArray() ) );
    } else {
        // Parse the header part to know what we've got
        mail.setHeader( QMailMessageHeader( ls.left(pos).toQByteArray() ) );

        // Parse the remainder as content
        mail.partContainerImpl()->fromRfc2822( ls.mid(pos + 4) );
    }

    return mail;
}

/*! 
    \fn QMailMessage::serialize(Stream&) const
    \internal 
*/
template <typename Stream> 
void QMailMessage::serialize(Stream &stream) const
{
    metaDataImpl()->serialize(stream);
    partContainerImpl()->serialize(stream);
}

/*! 
    \fn QMailMessage::deserialize(Stream&)
    \internal 
*/
template <typename Stream> 
void QMailMessage::deserialize(Stream &stream)
{
    metaDataImpl()->deserialize(stream);
    partContainerImpl()->deserialize(stream);
}


Q_IMPLEMENT_USER_METATYPE_ENUM(QMailMessageBody::TransferEncoding)
Q_IMPLEMENT_USER_METATYPE_ENUM(QMailMessagePartContainer::MultipartType)
Q_IMPLEMENT_USER_METATYPE_ENUM(QMailMessage::MessageType)
Q_IMPLEMENT_USER_METATYPE_ENUM(QMailMessage::ContentType)
Q_IMPLEMENT_USER_METATYPE_ENUM(QMailMessage::AttachmentsAction)

Q_IMPLEMENT_USER_METATYPE(QMailMessage)
Q_IMPLEMENT_USER_METATYPE(QMailMessageMetaData)

Q_IMPLEMENT_USER_METATYPE_TYPEDEF(QMailMessageList, QMailMessageList)
Q_IMPLEMENT_USER_METATYPE_TYPEDEF(QMailMessageMetaDataList, QMailMessageMetaDataList)
Q_IMPLEMENT_USER_METATYPE_TYPEDEF(QMailMessageTypeList, QMailMessageTypeList)

