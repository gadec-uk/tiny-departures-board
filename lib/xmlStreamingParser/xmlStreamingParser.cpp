/*
 * XML Streaming Parser Library
 *  - based on the structure of samxl embedded XML parser by Zorxx Software at https://github.com/zorxx/saxml
 *
 * MIT License
 *
 * Copyright (c) 2025-2026 Gadec Software
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */
#include <xmlStreamingParser.h>

#define fallthrough  __attribute__((__fallthrough__))

xmlStreamingParser::xmlStreamingParser() {
    reset();
}

void xmlStreamingParser::setListener(xmlListener* listener) {
    myListener = listener;
}

void xmlStreamingParser::reset() {
    inAttrQuote=false;
    cdataIndex = 0;
    ChangeState(STATE_BEGIN);
}

void xmlStreamingParser::parse(const char character) {
    switch (state) {
        case STATE_BEGIN:
            state_Begin(character);
            break;
        case STATE_STARTTAG:
            state_StartTag(character);
            break;
        case STATE_TAGNAME:
            state_TagName(character);
            break;
        case STATE_TAGCONTENTS:
            state_TagContents(character);
            break;
        case STATE_ENDTAG:
            state_EndTag(character);
            break;
        case STATE_ATTRIBUTE:
            state_Attribute(character);
            break;
        case STATE_CDATA:
            state_CDATA(character);
            break;
        case STATE_COMMENT:
            state_Comment(character);
            break;
        default:
            break;
    }
}

/* Wait for a tag start character */
void  xmlStreamingParser::state_Begin(const char character) {

    if (bInitialize) {
        length=0;
        buffer[length] = '\0';
        bInitialize=false;
    }

    switch (character)
    {
        case '<':
            ChangeState(STATE_STARTTAG);
            break;
        default:
            break;
    }
}

/* We've already found a tag start character, determine if this is start or end tag,
 *  and parse the tag name */
void xmlStreamingParser::state_StartTag(const char character) {

    static const char cdataStart[] = "<![CDATA[";
    static const char commentStart[] = "<!--";

    if (bInitialize) {
        bInitialize = false;
        cdataIndex = 0;
        cdataMatch[cdataIndex++] = '<';   // '<' already consumed
    }

    // Accumulate possible special sequence
    cdataMatch[cdataIndex++] = character;
    cdataMatch[cdataIndex] = '\0';

    // Check for CDATA
    if (strncmp(cdataMatch, cdataStart, cdataIndex) == 0) {
        if (cdataIndex == 9) {
            length = 0;
            buffer[length] = '\0';
            ChangeState(STATE_CDATA);
        }
        return;
    }

    // Check for comment
    if (strncmp(cdataMatch, commentStart, cdataIndex) == 0) {
        if (cdataIndex == 4) {
            ChangeState(STATE_COMMENT);
        }
        return;
    }

    // Not a special tag (CDATA or comment)
    cdataIndex = 0;

    switch(character)
    {
        case '<': case '>':
            /* Syntax error! */
            break;
        case ' ': case '\r': case '\n': case '\t':
            /* Ignore whitespace */
            break;
        case '/':
            ChangeState(STATE_ENDTAG);
            break;
        default:
            buffer[0] = character;
            length = 1;
            buffer[length] = '\0';
            ChangeState(STATE_TAGNAME);
            break;
    }
}

void xmlStreamingParser::state_TagName(const char character) {

    static bool sawSlash = false;
    nextState = STATE_NULL;

    if (bInitialize) {
        bInitialize = false;
        sawSlash = false;
    }

    switch (character)
    {
        case ' ': case '\r': case '\n': case '\t':
            nextState = STATE_ATTRIBUTE;
            break;

        case '/':
            // Possible empty tag; wait to see '>'
            sawSlash = true;
            break;

        case '>':
            if (sawSlash) {
                // Self-closing tag without attributes
                // startTag already emitted below
                nextState = STATE_TAGCONTENTS;
            } else {
                nextState = STATE_TAGCONTENTS;
            }
            break;

        default:
            if (sawSlash) {
                // '/' was actually part of tag name
                ContextBufferAddChar('/');
                sawSlash = false;
            }
            ContextBufferAddChar(character);
            break;
    }

    if (nextState != STATE_NULL) {
        if (length > 0) {
            // Copy tag name for later endTag (empty tags)
            //strcpy(currentTagName, buffer);

            // Emit startTag exactly once here
            myListener->startTag(buffer);
        }
        ChangeState(nextState);
    }
}

void xmlStreamingParser::state_TagContents(const char character) {
    nextState = STATE_NULL;

    if(bInitialize)
    {
        length = 0;
        buffer[length] = '\0';
        bInitialize = false;
    }

    switch(character)
    {
        case '<':
            if (length>0) {
                length++;
                myListener->value(buffer);
            }
            cdataIndex = 0;
            cdataMatch[cdataIndex++] = '<';
            ChangeState(STATE_STARTTAG);
            return;
        case ' ': case '\r': case '\n': case '\t':
            if(length == 0)
                break; /* Ignore leading whitespace */
            else
            {
                // Fallthrough
                fallthrough;
            }
        default:
            ContextBufferAddChar(character);
            break;
    }

    if(nextState != STATE_NULL)
    {
        if (length>0) { length++; myListener->value(buffer); }
        ChangeState(nextState);
    }
}

void xmlStreamingParser::state_Attribute(const char character) {

    static bool sawSlash = false;
    nextState = STATE_NULL;

    if (bInitialize) {
        length = 0;
        buffer[length] = '\0';
        inAttrQuote = false;
        sawSlash = false;
        bInitialize = false;
    }

    switch (character)
    {
        case ' ': case '\r': case '\n': case '\t':
            if (!inAttrQuote && length > 0) {
                myListener->attribute(buffer);
                length = 0;
                buffer[length] = '\0';
            }
            break;

        case '\"':
            inAttrQuote = !inAttrQuote;
            ContextBufferAddChar(character);
            break;

        case '/':
            if (inAttrQuote) {
                ContextBufferAddChar('/');
            } else {
                sawSlash = true; // possible empty tag
            }
            break;

        case '>':
            if (length > 0) {
                myListener->attribute(buffer);
                length = 0;
                buffer[length] = '\0';
            }

            if (sawSlash) {
                // SELF-CLOSING TAG
                // Only emit endTag here (startTag already called)
                // myListener->endTag(currentTagName);
                myListener->endTag("");
                sawSlash = false;
            }

            nextState = STATE_TAGCONTENTS;
            break;

        default:
            if (sawSlash) {
                // '/' was part of attribute value or name
                ContextBufferAddChar('/');
                sawSlash = false;
            }
            ContextBufferAddChar(character);
            break;
    }

    if (nextState != STATE_NULL) {
        ChangeState(nextState);
    }
}

void xmlStreamingParser::state_EndTag(const char character) {

    nextState=STATE_NULL;

    if(bInitialize)
    {
        length = 0;
        buffer[length] = '\0';
        bInitialize = false;
    }

    switch(character)
    {
        case '<':
            /* Syntax error! */
            break;
        case ' ': case '\r': case '\n': case '\t':
            /* Ignore whitespace */
            break;
        case '>':
            nextState = STATE_TAGCONTENTS;
            break;
        default:
            ContextBufferAddChar(character);
            break;
    }

    if(nextState != STATE_NULL)
    {
        if (length>0) { length++; myListener->endTag(buffer); }
        ChangeState(nextState);
    }
}

void xmlStreamingParser::state_CDATA(const char character) {
    static int endMatch = 0;
    const char cdataEnd[] = "]]>";

    if (character == cdataEnd[endMatch]) {
        endMatch++;
        if (endMatch == 3) {
            // End of CDATA
            if (length > 0) {
                length++;
                myListener->value(buffer);
            }
            endMatch = 0;
            ChangeState(STATE_TAGCONTENTS);
        }
        return;
    }

    if (endMatch > 0) {
        // Partial match failed; flush buffered ]
        for (int i = 0; i < endMatch; i++) {
            ContextBufferAddChar(']');
        }
        endMatch = 0;
    }
    if (character != '\r' && character != '\n') ContextBufferAddChar(character);
}

void xmlStreamingParser::state_Comment(const char character) {
    static int endMatch = 0;
    const char commentEnd[] = "-->";

    if (character == commentEnd[endMatch]) {
        endMatch++;
        if (endMatch == 3) {
            endMatch = 0;
            ChangeState(STATE_TAGCONTENTS);
        }
        return;
    }

    endMatch = 0; // ignore everything
}

void xmlStreamingParser::ContextBufferAddChar(const char character) {
    if (length < sizeof(buffer)-2) {
        buffer[length] = character;
        length++;
        buffer[length] = '\0';
    }
}

void xmlStreamingParser::ChangeState(int newState) {
    state = newState;
    bInitialize=true;
}