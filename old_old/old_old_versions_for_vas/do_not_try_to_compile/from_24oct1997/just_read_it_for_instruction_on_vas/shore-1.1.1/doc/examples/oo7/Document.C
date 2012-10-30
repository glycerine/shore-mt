/*$Header: /p/shore/shore_cvs/src/oo7/Document.C,v 1.8 1995/03/24 23:57:52 nhall Exp $*/
/********************************************************/
/*                                                      */
/*               OO7 Benchmark                          */
/*                                                      */
/*              COPYRIGHT (C) 1993                      */
/*                                                      */
/*                Michael J. Carey 		        */
/*                David J. DeWitt 		        */
/*                Jeffrey Naughton 		        */
/*               Madison, WI U.S.A.                     */
/*                                                      */
/*	         ALL RIGHTS RESERVED                    */
/*                                                      */
/********************************************************/

#include <libc.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "oo7.h"
#include "globals.h"
#include "GenParams.h"
#include "VarParams.h"

////////////////////////////////////////////////////////////////////////////
//
// Document Methods
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// Document Constructor
//
////////////////////////////////////////////////////////////////////////////

void
Document::init(long cpId, REF(CompositePart) cp)
{
    char 	*p;
    char	myText[DocTextLength];

    // add document to extent of all documents
    // dts 3-11-93 AllDocuments is never used, so don't maintain it.
    // AllDocuments.add(this);

    // fill in id
    id = cpId; 
    // fill in back pointer to composite part
    part = cp;

    // prepare and fill in the document title
    char docTitle[TitleSize];
    sprintf(docTitle, "Composite Part %08d", cpId);
    if (debugMode) {
        printf("Document::Document(title = %s)\n", docTitle);
    }
    strncpy(title, docTitle, (unsigned int)TitleSize);

    // prepare and fill in the document text
    sprintf(myText, DocumentText, cpId);
    int stringLen = strlen(myText);

    // allocate the space to hold the actual document
    // SDL: unknown what to do with this.
    // p = (dbchar *) new (DocumentSet) dbchar[(DocumentSize)];
    // fprintf(stderr,"allocated document as transient ptr");
    p = new char[DocumentSize];

    // initialize the document
    int curChar, strChar;
    curChar = strChar = 0;
    while (curChar < DocumentSize-1) {
        p[curChar++] = myText[strChar++];
        if (strChar >= stringLen) { strChar = 0; }
    }
    p[curChar] = '\0';

    // now make the document object point at the actual document text
    // text = p; 
    text.set(p);
    
}

// destructor
void
Document::Delete()
{
    // SDL help needed here.
    // delete text;  // delete actual document text
    text.kill();
    // remove document from extent of all documents
    // dts 3-11-93 AllDocuments is never used, so don't maintain it.
    // AllDocuments.remove (this);
    // now delete the object.
    SH_DO(get_ref().destroy());
}

////////////////////////////////////////////////////////////////////////////
//
// Document searchText Method for use in traversals
//
////////////////////////////////////////////////////////////////////////////

long Document::searchText(char c) const
{
    char x;
	
    if (debugMode) {
        printf("                    Document::searchText(title = %s)\n", title);
    }
    x = c;

    // count occurrences of the indicated letter (for busy work)
    int i = 0; 
    int count = 0;
    count = text.countc(c);
    // do some manual cse elimination for E
    //{
	//char *text = this->text;
    	//while (text[i] != '\0') {
	    //if (text[i++] == x) { count++; }
    	//}
    //}

    if (debugMode) {
        printf("                    [found %d '%c's among %d characters]\n",
	        		     count, c, i);
    }
    return count;
}


////////////////////////////////////////////////////////////////////////////
//
// Document replaceText Method for use in traversals
//
////////////////////////////////////////////////////////////////////////////

long Document::replaceText(char *oldString, char *newString)
{
    // char *newText;
    // int   newSize;

    if (debugMode) {
        printf("                    Document::changeText(title = %s)\n", title);
    }

    // check to see if the text starts with the old string

    // int oldTextLength = strlen(text) + 1;
    int oldTextLength = text.strlen() + 1;
    int oldStrLength  = strlen(oldString);
    // int foundMatch = (strncmp(text, oldString, oldStrLength) == 0);
    int foundMatch = (text.strncmp(oldString, oldStrLength) == 0);

    // if so, change it to start with the new string instead
    if (foundMatch) 
    {
        int newStrLength  = strlen(newString);
        int lengthDiff = newStrLength - oldStrLength;
        int newTextLength  = oldTextLength + lengthDiff;

	if (lengthDiff == 0) {
            // strncpy(text, newString, newStrLength);
	    //text.strncpy( newString, newStrLength);
	    text.set(newString,0,newStrLength);
        } else {
	   // ok, ugly brute force method.  create a new temp string
	   // and assign it.
	   char * newstr = new char[newTextLength];
	   strcpy(newstr,newString);
	   strcpy(newstr+newStrLength,((char *)text) +oldStrLength);
	   text.set(newString,0,newStrLength);
	   delete newstr;

	}
    }

    if (debugMode) 
    {
	if (foundMatch) {
            printf("                    [changed \"%s\" to \"%s\"]\n",
	                                 oldString, newString);
	} else {
            printf("                    [no match, so no change was made]\n");
	}
    }

    if (foundMatch) {
        return 1;

    } else {
        return 0;
    }
}


