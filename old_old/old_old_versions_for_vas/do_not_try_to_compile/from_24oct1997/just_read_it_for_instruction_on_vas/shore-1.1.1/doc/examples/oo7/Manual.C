/*$Header: /p/shore/shore_cvs/src/oo7/Manual.C,v 1.7 1996/07/24 21:18:58 nhall Exp $*/
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
// Manual Methods
//
////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////
//
// Manual Constructor
//
////////////////////////////////////////////////////////////////////////////

void
Manual::init(long modId, REF(Module) myModule)
{
    char 	*p;
    char	myText[ManualTextLength];

    // fill in id
    id = modId; 
    // fill in back pointer to composite part
    mod = myModule;

    // prepare and fill in the document title
    char docTitle[TitleSize];
    sprintf(docTitle, "Manual         %08d", modId);
    if (debugMode) {
        printf("Manual::Manual(title = %s)\n", docTitle);
    }
    strncpy(title, docTitle, (unsigned int)TitleSize);

    // prepare and fill in the document text
    sprintf(myText, ManualText, modId);
    int stringLen = strlen(myText);

    // allocate the space to hold the actual document
    //p = (dbchar *) new (ManualSet) dbchar[(ManualSize)];
    // fprintf(stderr,"allocating transient mem for manual");
    p = new char[ManualSize];

    // initialize the document
    int curChar, strChar;
    curChar = strChar = 0;
    while (curChar < ManualSize-1) {
        p[curChar++] = myText[strChar++];
        if (strChar >= stringLen) { strChar = 0; }
    }
    p[curChar] = '\0';

    // now make the manual object point at the actual document text
    // text = p; 
    // set the len field
    //text.strcpy(p);
    text.set(p);
    textLen = stringLen;
}

// destructor
void
Manual::Delete()
{
    // delete text;  // delete actual manual text
    //text.Delete();
    text.kill();
    // remove document from extent of all documents
//    AllDocuments.remove (this);
}

////////////////////////////////////////////////////////////////////////////
//
// Manual searchText Method for use in traversals
//
////////////////////////////////////////////////////////////////////////////

long Manual::searchText(char c) const
{
    char x;
	
    if (debugMode) {
        printf("                    Manual::searchText(title = %s)\n", title);
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
// Manual replaceText Method for use in traversals
//
////////////////////////////////////////////////////////////////////////////

long Manual::replaceText(char *oldString, char *newString)
{
    //char *newText;
    //int   newSize;

    if (debugMode) {
        printf("                    Manual::changeText(title = %s)\n", title);
    }

    // check to see if the text starts with the old string

    // int oldTextLength = strlen(text) + 1;
    int oldTextLength = text.strlen() + 1;
    int oldStrLength  = strlen(oldString);
    //int foundMatch = (strncmp(text, oldString, oldStrLength) == 0);
    int foundMatch = (text.strncmp(oldString, oldStrLength) == 0);

    // if so, change it to start with the new string instead
    if (foundMatch) 
    {
        int newStrLength  = strlen(newString);
        int lengthDiff = newStrLength - oldStrLength;
        int newTextLength  = oldTextLength + lengthDiff;

	if (lengthDiff == 0) {
            //text.strncpy( newString, newStrLength);
	    text.set(newString);
            // strncpy(text, newString, newStrLength);
        } else {
	// delete/insert method: delete the old string and insert new
	// one, using bins/bdel
	   // bdel(text,oldStrLength);
	   // bins(text,newString,newStrLength);
	   text.set(newString,0,oldStrLength);
	   //char * newstr = new char[newTextLength];
	   //strcpy(newstr,newString);
	   //strcpy(newstr,&text[oldStrLength]);
	   //delete text;
	   //text = newstr;
	   fprintf(stderr,"manual: put transient ptr into persistent space");
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

////////////////////////////////////////////////////////////////////////////
//
// Manual firstLast Method for use in traversals
//
////////////////////////////////////////////////////////////////////////////

long Manual::firstLast() const
{
    size_t len;

    len = text.strlen();
    return text.get(size_t(0)) == text.get(len - 1);
    //jk return text.get((long unsigned int)size_t(0)) == text.get((long unsigned int)(len - 1));
    // return (text[0] == text[(int)textLen-1]);
}
