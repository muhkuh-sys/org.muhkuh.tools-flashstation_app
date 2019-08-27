#include "snuprintf.h"

#include <string.h>

#include "netx_io_areas.h"


typedef struct SNUPRINTF_BUFFER_STRUCT
{
	char *pucCnt;
	char *pucEnd;
} SNUPRINTF_BUFFER_T;


/* This is the routine for the simulation. */
static void print_char(SNUPRINTF_BUFFER_T *ptBuf, char cData)
{
	char *pucCnt;


	pucCnt = ptBuf->pucCnt;
	if( pucCnt<ptBuf->pucEnd )
	{
		*(pucCnt++) = cData;
		ptBuf->pucCnt = pucCnt;
	}
}



static void snuprintf_hex(SNUPRINTF_BUFFER_T *ptBuf, unsigned long ulValue, unsigned int sizMinimum, char cFillUpChar)
{
	int fLeadingDigitWasPrinted;
	unsigned int sizCnt;
	unsigned int uiDigit;


	/* the maximum size is 8 chars */
	sizCnt = 8;

	/* no leading digit was printed yet */
	fLeadingDigitWasPrinted = 0;

	/* loop over all possible chars */
	do
	{
		--sizCnt;

		/* get top digit */
		uiDigit = ulValue >> 28;
		if( uiDigit==0 && fLeadingDigitWasPrinted==0 && sizCnt>0 )
		{
			/* already reached the minimum display size? */
			if( sizCnt<sizMinimum )
			{
				/* yes -> print fill-up char */
				print_char(ptBuf, cFillUpChar);
			}
		}
		else
		{
			/* print the digit */
			uiDigit |= '0';
			if( uiDigit>'9' )
			{
				uiDigit += 'a'-'9'-1;
			}
			print_char(ptBuf, (unsigned char)uiDigit);

			/* now the leading digit has been printed */
			fLeadingDigitWasPrinted = 1;
		}

		/* move to next digit */
		ulValue <<= 4;
	} while( sizCnt>0 );
}


static const unsigned long aulSnuprintfDecTab[10] =
{
	         1,
	        10,
	       100,
	      1000,
	     10000,
	    100000,
	   1000000,
	  10000000,
	 100000000,
	1000000000
};

static void snuprintf_dec(SNUPRINTF_BUFFER_T *ptBuf, unsigned long ulValue, unsigned int sizMinimum, char cFillUpChar)
{
	int fLeadingDigitWasPrinted;
	unsigned int sizCnt;
	unsigned int uiDigit;


	/* the maximum size is 10 chars */
	sizCnt = 10;

	/* no leading digit was printed yet */
	fLeadingDigitWasPrinted = 0;

	/* loop over all possible chars */
	do
	{
		--sizCnt;

		/* get top digit */
		uiDigit = 0;
		while( ulValue>=aulSnuprintfDecTab[sizCnt] )
		{
			ulValue -= aulSnuprintfDecTab[sizCnt];
			++uiDigit;
		}

		if( uiDigit==0 && fLeadingDigitWasPrinted==0 && sizCnt>0 )
		{
			/* already reached the minimum display size? */
			if( sizCnt<sizMinimum )
			{
				/* yes -> print fill-up char */
				print_char(ptBuf, cFillUpChar);
			}
		}
		else
		{
			/* print the digit */
			uiDigit |= '0';
			print_char(ptBuf, (unsigned char)uiDigit);

			/* now the leading digit has been printed */
			fLeadingDigitWasPrinted = 1;
		}
	} while( sizCnt>0 );
}


static void snuprintf_bin(SNUPRINTF_BUFFER_T *ptBuf, unsigned long ulValue, unsigned int sizMinimum, char cFillUpChar)
{
	int fLeadingDigitWasPrinted;
	unsigned int sizCnt;
	unsigned int uiDigit;


	/* the maximum size is 32 chars */
	sizCnt = 32;

	/* no leading digit was printed yet */
	fLeadingDigitWasPrinted = 0;

	/* loop over all possible chars */
	do
	{
		--sizCnt;

		/* get top digit */
		uiDigit = ulValue >> 31;
		if( uiDigit==0 && fLeadingDigitWasPrinted==0 && sizCnt>0 )
		{
			/* already reached the minimum display size? */
			if( sizCnt<sizMinimum )
			{
				/* yes -> print fill-up char */
				print_char(ptBuf, cFillUpChar);
			}
		}
		else
		{
			/* print the digit */
			uiDigit |= '0';
			print_char(ptBuf, (unsigned char)uiDigit);

			/* now the leading digit has been printed */
			fLeadingDigitWasPrinted = 1;
		}

		/* move to next digit */
		ulValue <<= 1;
	} while( sizCnt>0 );
}


static void snuprintf_str(SNUPRINTF_BUFFER_T *ptBuf, const char *pcString, unsigned int sizMinimum, char cFillUpChar)
{
	unsigned int sizString;
	unsigned int sizCnt;
	const char *pcCnt;
	char cChar;


	/* get the length of the string */
	sizString = 0;
	pcCnt = pcString;
	while( *(pcCnt++)!='\0' )
	{
		++sizString;
	}

	/* fill-up string if it is smaller than the requested size */
	sizCnt = sizString;
	while( sizCnt<sizMinimum )
	{
		print_char(ptBuf, cFillUpChar);
		++sizCnt;
	}

	/* print the string */
	pcCnt = pcString;
	do
	{
		cChar = *(pcCnt++);
		if( cChar!='\0' )
		{
			print_char(ptBuf, cChar);
		}
	} while( cChar!='\0' );
}


unsigned int snuprintf(char *pcBuffer, unsigned int sizBuffer, const char *pcFmt, ...)
{
	char cChar;
	va_list ptArgument;
	char cFillUpChar;
	unsigned int sizMinimumSize;
	int iDigitCnt;
	unsigned int uiCnt;
	unsigned int uiValue;
	const char *pcNumCnt;
	const char *pcNumEnd;
	unsigned long ulArgument;
	const char *pcArgument;
	int iArgument;
	SNUPRINTF_BUFFER_T tBuf;
	unsigned int sizText;


	/* Get initial pointer to first argument */
	va_start(ptArgument, pcFmt);

	/* Is it a NULL Pointer ? */
	if( pcFmt==NULL )
	{
		/* replace the argument with the default string */
		pcFmt = "NULL\n";
	}

	/* Initialize the buffer structure. */
	tBuf.pucCnt = pcBuffer;
	tBuf.pucEnd = pcBuffer + sizBuffer;

	/* Loop over all chars in the format string. */
	do
	{
		/* Get the next char. */
		cChar = *(pcFmt++);
	
		/* Is this the end of the format string? */
		if( cChar!=0 )
		{
			/* No -> process the char. */
	
			/* Is this an escape char? */
			if( cChar=='%' )
			{
				/* Yes -> process the escape sequence. */

				/* Set default values for escape sequences. */
				cFillUpChar = ' ';
				sizMinimumSize = 0;
	
				do
				{
					cChar = *(pcFmt++);
					if( cChar=='%' )
					{
						/* It is just a '%'. */
						print_char(&tBuf, cChar);
						break;
					}
					else if( cChar=='0' )
					{
						cFillUpChar = '0';
					}
					else if( cChar>'0' && cChar<='9' )
					{
						/* No digit found yet. */
						iDigitCnt = 1;
						/* The number started one char before. */
						pcNumEnd = pcFmt;
						/* Count all digits. */
						do
						{
							cChar = *pcFmt;
							if( cChar>='0' && cChar<='9' )
							{
								++pcFmt;
							}
							else
							{
								break;
							}
						} while(1);
	
						/* Loop over all digits and add them to the value. */
						uiValue = 0;
						iDigitCnt = 0;
						pcNumCnt = pcFmt;
						while( pcNumCnt>=pcNumEnd )
						{
							--pcNumCnt;
							uiCnt = (*pcNumCnt) & 0x0fU;
							while( uiCnt>0 )
							{
								uiValue += aulSnuprintfDecTab[iDigitCnt];
								--uiCnt;
							}
							++iDigitCnt;
						}
						sizMinimumSize = uiValue;
					}
					else if( cChar=='x' )
					{
						/* Show a hexadecimal number. */
						ulArgument = va_arg((ptArgument), unsigned long);
						snuprintf_hex(&tBuf, ulArgument, sizMinimumSize, cFillUpChar);
						break;
					}
					else if( cChar=='d' )
					{
						/* Show a decimal number. */
						ulArgument = va_arg((ptArgument), unsigned long);
						snuprintf_dec(&tBuf, ulArgument, sizMinimumSize, cFillUpChar);
						break;
					}
					else if( cChar=='b' )
					{
						/* Show a binary number. */
						ulArgument = va_arg((ptArgument), unsigned long);
						snuprintf_bin(&tBuf, ulArgument, sizMinimumSize, cFillUpChar);
						break;
					}
					else if( cChar=='s' )
					{
						/* Show a string. */
						pcArgument = va_arg((ptArgument), const char *);
						snuprintf_str(&tBuf, pcArgument, sizMinimumSize, cFillUpChar);
						break;
					}
					else if( cChar=='c' )
					{
						/* Show a char. */
						iArgument = va_arg((ptArgument), int);
						print_char(&tBuf, (char)iArgument);
						break;
					}
					else
					{
						print_char(&tBuf, '*');
						print_char(&tBuf, '*');
						print_char(&tBuf, '*');
						break;
					}
				} while( cChar!=0 );
			}
			else
			{
				print_char(&tBuf, cChar);
			}
		}
	} while( cChar!=0 );

	va_end(ptArgument);

	/* Get the size of the text. */
	sizText = (unsigned int)(tBuf.pucCnt - pcBuffer);

	/* Add a terminating 0 to the buffer if there is still enough
	 * space. Do not add this to the size.
	 */
	if( tBuf.pucCnt<tBuf.pucEnd )
	{
		*(tBuf.pucCnt) = 0;
	}

	/* Return the complete size of the string. */
	return sizText;
}
