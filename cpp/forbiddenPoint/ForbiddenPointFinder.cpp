
// ForbiddenPointFinder.cpp: implementation of the CForbiddenPointFinder class.
//
// Code written by: Wenzhe Lu
//
//////////////////////////////////////////////////////////////////////

//#include "stdafx.h"
#include "ForbiddenPointFinder.h"
#include <algorithm>

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CForbiddenPointFinder::CForbiddenPointFinder() :f_boardsize(15)
{
	Clear();
}
CForbiddenPointFinder::CForbiddenPointFinder(int size) : f_boardsize(size)
{
	Clear();
}

CForbiddenPointFinder::~CForbiddenPointFinder()
{
}

//////////////////////////////////////////////////////////////////////
// Implementation
//////////////////////////////////////////////////////////////////////

void CForbiddenPointFinder::Clear()
{
	//nForbiddenPoints = 0;

	for (int i=0; i<f_boardsize+2; i++)
	{
		cBoard[0][i] = '$';
		cBoard[(f_boardsize+1)][i] = '$';
		cBoard[i][0] = '$';
		cBoard[i][(f_boardsize+1)] = '$';
	}
	
	for (int i=1; i<=f_boardsize; i++)
		for (int j=1; j<=f_boardsize; j++)
			cBoard[i][j] = C_EMPTY;
}

int CForbiddenPointFinder::AddStone(int x, int y, char cStone)//这个函数没用过
{
	int nResult = -1;

	if (cStone == C_BLACK)
	{
		if (IsFive(x, y, C_BLACK))
			nResult = BLACKFIVE;
		if(isForbidden( x,  y))nResult = BLACKFORBIDDEN;
		/*for (int i=0; i<nForbiddenPoints; i++)
		{
			if (ptForbidden[i] == CPoint(x,y))
				nResult = BLACKFORBIDDEN;
		}*/
	}
	else if (cStone == C_WHITE)
	{
		if (IsFive(x, y, C_WHITE))
			nResult = WHITEFIVE;
	}
	
	cBoard[x+1][y+1] = cStone;
	//if (nResult == -1)
	//	FindForbiddenPoints();
	//else
	//	nForbiddenPoints = 0;
	return nResult;
}

void CForbiddenPointFinder::SetStone(int x, int y, char cStone)
{
	cBoard[x+1][y+1] = cStone;
}

bool CForbiddenPointFinder::IsFive(int x, int y, int nColor)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return false;

	if (nColor == C_BLACK)	// black
	{
		SetStone(x, y, C_BLACK);
		
		// detect black five
		int i, j;
		
		// 1 - horizontal direction
		int nLine = 1;
		i = x;
		while (i > 0)
		{
			if (cBoard[i--][y+1] == C_BLACK)
				nLine++;
			else
				break;
		}
		i = x+2;
		while (i < (f_boardsize+1))
		{
			if (cBoard[i++][y+1] == C_BLACK)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		// 2 - vertical direction
		nLine = 1;
		i = y;
		while (i > 0)
		{
			if (cBoard[x+1][i--] == C_BLACK)
				nLine++;
			else
				break;
		}
		i = y+2;
		while (i < (f_boardsize+1))
		{
			if (cBoard[x+1][i++] == C_BLACK)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		// 3 - diagonal direction (lower-left to upper-right: '/')
		nLine = 1;
		i = x;
		j = y;
		while ((i > 0) && (j > 0))
		{
			if (cBoard[i--][j--] == C_BLACK)
				nLine++;
			else
				break;
		}
		i = x+2;
		j = y+2;
		while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
		{
			if (cBoard[i++][j++] == C_BLACK)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		// 4 - diagonal direction (upper-left to lower-right: '\')
		nLine = 1;
		i = x;
		j = y+2;
		while ((i > 0) && (j < (f_boardsize+1)))
		{
			if (cBoard[i--][j++] == C_BLACK)
				nLine++;
			else
				break;
		}
		i = x+2;
		j = y;
		while ((i < (f_boardsize+1)) && (j > 0))
		{
			if (cBoard[i++][j--] == C_BLACK)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		SetStone(x, y, C_EMPTY);
		return false;
	}
	else if (nColor == C_WHITE)	// white
	{
		SetStone(x, y, C_WHITE);
		
		// detect white five or more
		int i, j;
		
		// 1 - horizontal direction
		int nLine = 1;
		i = x;
		while (i > 0)
		{
			if (cBoard[i--][y+1] == C_WHITE)
				nLine++;
			else
				break;
		}
		i = x+2;
		while (i < (f_boardsize+1))
		{
			if (cBoard[i++][y+1] == C_WHITE)
				nLine++;
			else
				break;
		}
		if (nLine >= 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		// 2 - vertical direction
		nLine = 1;
		i = y;
		while (i > 0)
		{
			if (cBoard[x+1][i--] == C_WHITE)
				nLine++;
			else
				break;
		}
		i = y+2;
		while (i < (f_boardsize+1))
		{
			if (cBoard[x+1][i++] == C_WHITE)
				nLine++;
			else
				break;
		}
		if (nLine >= 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		// 3 - diagonal direction (lower-left to upper-right: '/')
		nLine = 1;
		i = x;
		j = y;
		while ((i > 0) && (j > 0))
		{
			if (cBoard[i--][j--] == C_WHITE)
				nLine++;
			else
				break;
		}
		i = x+2;
		j = y+2;
		while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
		{
			if (cBoard[i++][j++] == C_WHITE)
				nLine++;
			else
				break;
		}
		if (nLine >= 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		// 4 - diagonal direction (upper-left to lower-right: '\')
		nLine = 1;
		i = x;
		j = y+2;
		while ((i > 0) && (j < (f_boardsize+1)))
		{
			if (cBoard[i--][j++] == C_WHITE)
				nLine++;
			else
				break;
		}
		i = x+2;
		j = y;
		while ((i < (f_boardsize+1)) && (j > 0))
		{
			if (cBoard[i++][j--] == C_WHITE)
				nLine++;
			else
				break;
		}
		if (nLine >= 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}

		SetStone(x, y, C_EMPTY);
		return false;
	}
	else 
		return false;
}

bool CForbiddenPointFinder::IsOverline(int x, int y)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return false;

	SetStone(x, y, C_BLACK);
		
	// detect black overline
	int i, j;
	bool bOverline = false;
		
	// 1 - horizontal direction
	int nLine = 1;
	i = x;
	while (i > 0)
	{
		if (cBoard[i--][y+1] == C_BLACK)
			nLine++;
		else
			break;
	}
	i = x+2;
	while (i < (f_boardsize+1))
	{
		if (cBoard[i++][y+1] == C_BLACK)
			nLine++;
		else
			break;
	}
	if (nLine == 5)
	{
		SetStone(x, y, C_EMPTY);
		return false;
	}
	else
		bOverline |= (nLine >= 6);
		
	// 2 - vertical direction
	nLine = 1;
	i = y;
	while (i > 0)
	{
		if (cBoard[x+1][i--] == C_BLACK)
			nLine++;
		else
			break;
	}
	i = y+2;
	while (i < (f_boardsize+1))
	{
		if (cBoard[x+1][i++] == C_BLACK)
			nLine++;
		else
			break;
	}
	if (nLine == 5)
	{
		SetStone(x, y, C_EMPTY);
		return false;
	}
	else
		bOverline |= (nLine >= 6);

	// 3 - diagonal direction (lower-left to upper-right: '/')
	nLine = 1;
	i = x;
	j = y;
	while ((i > 0) && (j > 0))
	{
		if (cBoard[i--][j--] == C_BLACK)
			nLine++;
		else
			break;
	}
	i = x+2;
	j = y+2;
	while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
	{
		if (cBoard[i++][j++] == C_BLACK)
			nLine++;
		else
			break;
	}
	if (nLine == 5)
	{
		SetStone(x, y, C_EMPTY);
		return false;
	}
	else
		bOverline |= (nLine >= 6);

	// 4 - diagonal direction (upper-left to lower-right: '\')
	nLine = 1;
	i = x;
	j = y+2;
	while ((i > 0) && (j < (f_boardsize+1)))
	{
		if (cBoard[i--][j++] == C_BLACK)
			nLine++;
		else
			break;
	}
	i = x+2;
	j = y;
	while ((i < (f_boardsize+1)) && (j > 0))
	{
		if (cBoard[i++][j--] == C_BLACK)
			nLine++;
		else
			break;
	}
	if (nLine == 5)
	{
		SetStone(x, y, C_EMPTY);
		return false;
	}
	else
		bOverline |= (nLine >= 6);

	SetStone(x, y, C_EMPTY);
	return bOverline;
}

bool CForbiddenPointFinder::IsFive(int x, int y, int nColor, int nDir)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return false;
	
	char c;
	if (nColor == C_BLACK)	// black
		c = C_BLACK;
	else if (nColor == C_WHITE)	// white
		c = C_WHITE;
	else
		return false;
		
	SetStone(x, y, c);

	int i, j;
	int nLine;
			
	switch (nDir)
	{
	case 1:		// horizontal direction
		nLine = 1;
		i = x;
		while (i > 0)
		{
			if (cBoard[i--][y+1] == c)
				nLine++;
			else
				break;
		}
		i = x+2;
		while (i < (f_boardsize+1))
		{
			if (cBoard[i++][y+1] == c)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}
		else
		{
			SetStone(x, y, C_EMPTY);
			return false;
		}
		break;
	case 2:		// vertial direction
		nLine = 1;
		i = y;
		while (i > 0)
		{
			if (cBoard[x+1][i--] == c)
				nLine++;
			else
				break;
		}
		i = y+2;
		while (i < (f_boardsize+1))
		{
			if (cBoard[x+1][i++] == c)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}
		else
		{
			SetStone(x, y, C_EMPTY);
			return false;
		}
		break;
	case 3:		// diagonal direction - '/'
		nLine = 1;
		i = x;
		j = y;
		while ((i > 0) && (j > 0))
		{
			if (cBoard[i--][j--] == c)
				nLine++;
			else
				break;
		}
		i = x+2;
		j = y+2;
		while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
		{
			if (cBoard[i++][j++] == c)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}
		else
		{
			SetStone(x, y, C_EMPTY);
			return false;
		}
		break;
	case 4:		// diagonal direction - '\'
		nLine = 1;
		i = x;
		j = y+2;
		while ((i > 0) && (j < (f_boardsize+1)))
		{
			if (cBoard[i--][j++] == c)
				nLine++;
			else
				break;
		}
		i = x+2;
		j = y;
		while ((i < (f_boardsize+1)) && (j > 0))
		{
			if (cBoard[i++][j--] == c)
				nLine++;
			else
				break;
		}
		if (nLine == 5)
		{
			SetStone(x, y, C_EMPTY);
			return true;
		}
		else
		{
			SetStone(x, y, C_EMPTY);
			return false;
		}
		break;
	default:
		SetStone(x, y, C_EMPTY);
		return false;
		break;
	}
}

bool CForbiddenPointFinder::IsFour(int x, int y, int nColor, int nDir)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return false;

	if (IsFive(x, y, nColor))	// five?
		return false;
	else if ((nColor == C_BLACK) && (IsOverline(x, y)))	// black overline?
		return false;
	else
	{
		char c;
		if (nColor == C_BLACK)	// black
			c = C_BLACK;
		else if (nColor == C_WHITE)	// white
			c = C_WHITE;
		else
			return false;
		
		SetStone(x, y, c);

		int i, j;
			
		switch (nDir)
		{
		case 1:		// horizontal direction
			i = x;
			while (i > 0)
			{
				if (cBoard[i][y+1] == c)
				{
					i--;
					continue;
				}
				else if (cBoard[i][y+1] == C_EMPTY)
				{
					if (IsFive(i-1, y, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = x+2;
			while (i < (f_boardsize+1))
			{
				if (cBoard[i][y+1] == c)
				{
					i++;
					continue;
				}
				else if (cBoard[i][y+1] == C_EMPTY)
				{
					if (IsFive(i-1, y, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		case 2:		// vertial direction
			i = y;
			while (i > 0)
			{
				if (cBoard[x+1][i] == c)
				{
					i--;
					continue;
				}
				else if (cBoard[x+1][i] == C_EMPTY)
				{
					if (IsFive(x, i-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = y+2;
			while (i < (f_boardsize+1))
			{
				if (cBoard[x+1][i] == c)
				{
					i++;
					continue;
				}
				else if (cBoard[x+1][i] == C_EMPTY)
				{
					if (IsFive(x, i-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		case 3:		// diagonal direction - '/'
			i = x;
			j = y;
			while ((i > 0) && (j > 0))
			{
				if (cBoard[i][j] == c)
				{
					i--;
					j--;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = x+2;
			j = y+2;
			while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
			{
				if (cBoard[i][j] == c)
				{
					i++;
					j++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		case 4:		// diagonal direction - '\'
			i = x;
			j = y+2;
			while ((i > 0) && (j < (f_boardsize+1)))
			{
				if (cBoard[i][j] == c)
				{
					i--;
					j++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = x+2;
			j = y;
			while ((i < (f_boardsize+1)) && (j > 0))
			{
				if (cBoard[i][j] == c)
				{
					i++;
					j--;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		default:
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		}
	}
}

int CForbiddenPointFinder::IsOpenFour(int x, int y, int nColor, int nDir)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return 0;
	
	if (IsFive(x, y, nColor))	// five?
		return 0;
	else if ((nColor == C_BLACK) && (IsOverline(x, y)))	// black overline?
		return 0;
	else
	{
		char c;
		if (nColor == C_BLACK)	// black
			c = C_BLACK;
		else if (nColor == C_WHITE)	// white
			c = C_WHITE;
		else
			return 0;
		
		SetStone(x, y, c);

		int i, j;
		int nLine;
			
		switch (nDir)
		{
		case 1:		// horizontal direction
			nLine = 1;
			i = x;
			while (i >= 0)
			{
				if (cBoard[i][y+1] == c)
				{
					i--;
					nLine++;
					continue;
				}
				else if (cBoard[i][y+1] == C_EMPTY)
				{
					if (!IsFive(i-1, y, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return 0;
					}
					else 
						break;
				}
				else
				{
					SetStone(x, y, C_EMPTY);
					return 0;
				}
			}
			i = x+2;
			while (i < (f_boardsize+1))
			{
				if (cBoard[i][y+1] == c)
				{
					i++;
					nLine++;
					continue;
				}
				else if (cBoard[i][y+1] == C_EMPTY)
				{
					if (IsFive(i-1, y, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return (nLine==4 ? 1 : 2);
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return 0;
			break;
		case 2:		// vertial direction
			nLine = 1;
			i = y;
			while (i >= 0)
			{
				if (cBoard[x+1][i] == c)
				{
					i--;
					nLine++;
					continue;
				}
				else if (cBoard[x+1][i] == C_EMPTY)
				{
					if (!IsFive(x, i-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return 0;
					}
					else 
						break;
				}
				else
				{
					SetStone(x, y, C_EMPTY);
					return 0;
				}
			}
			i = y+2;
			while (i < (f_boardsize+1))
			{
				if (cBoard[x+1][i] == c)
				{
					i++;
					nLine++;
					continue;
				}
				else if (cBoard[x+1][i] == C_EMPTY)
				{
					if (IsFive(x, i-1,c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return (nLine==4 ? 1 : 2);
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return 0;
			break;
		case 3:		// diagonal direction - '/'
			nLine = 1;
			i = x;
			j = y;
			while ((i >= 0) && (j >= 0))
			{
				if (cBoard[i][j] == c)
				{
					i--;
					j--;
					nLine++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (!IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return 0;
					}
					else 
						break;
				}
				else
				{
					SetStone(x, y, C_EMPTY);
					return 0;
				}
			}
			i = x+2;
			j = y+2;
			while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
			{
				if (cBoard[i][j] == c)
				{
					i++;
					j++;
					nLine++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return (nLine==4 ? 1 : 2);
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return 0;
			break;
		case 4:		// diagonal direction - '\'
			nLine = 1;
			i = x;
			j = y+2;
			while ((i >= 0) && (j <= (f_boardsize+1)))
			{
				if (cBoard[i][j] == c)
				{
					i--;
					j++;
					nLine++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (!IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return 0;
					}
					else 
						break;
				}
				else
				{
					SetStone(x, y, C_EMPTY);
					return 0;
				}
			}
			i = x+2;
			j = y;
			while ((i < (f_boardsize+1)) && (j > 0))
			{
				if (cBoard[i][j] == c)
				{
					i++;
					j--;
					nLine++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if (IsFive(i-1, j-1, c, nDir))
					{
						SetStone(x, y, C_EMPTY);
						return (nLine==4 ? 1 : 2);
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return 0;
			break;
		default:
			SetStone(x, y, C_EMPTY);
			return 0;
			break;
		}
	}
}

bool CForbiddenPointFinder::IsDoubleFour(int x, int y)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return false;

	if (IsFive(x, y, C_BLACK))	// five?
		return false;
	
	int nFour = 0;
	for (int i=1; i<=4; i++)
	{
		if (IsOpenFour(x, y, C_BLACK, i) == 2)
			nFour += 2;
		else if (IsFour(x, y, C_BLACK, i))
			nFour++;
	}

	if (nFour >= 2)
		return true;
	else
		return false;
}

bool CForbiddenPointFinder::IsOpenThree(int x, int y, int nColor, int nDir)
{
	if (IsFive(x, y, nColor))	// five?
		return false;
	else if ((nColor == C_BLACK) && (IsOverline(x, y)))	// black overline?
		return false;
	else
	{
		char c;
		if (nColor == C_BLACK)	// black
			c = C_BLACK;
		else if (nColor == C_WHITE)	// white
			c = C_WHITE;
		else
			return false;
		
		SetStone(x, y, c);

		int i, j;
			
		switch (nDir)
		{
		case 1:		// horizontal direction
			i = x;
			while (i > 0)
			{
				if (cBoard[i][y+1] == c)
				{
					i--;
					continue;
				}
				else if (cBoard[i][y+1] == C_EMPTY)
				{
					if ((IsOpenFour(i-1, y, nColor, nDir) == 1) && (!IsDoubleFour(i-1, y)) && (!IsDoubleThree(i-1, y)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = x+2;
			while (i < (f_boardsize+1))
			{
				if (cBoard[i][y+1] == c)
				{
					i++;
					continue;
				}
				else if (cBoard[i][y+1] == C_EMPTY)
				{
					if ((IsOpenFour(i-1, y, nColor, nDir) == 1) && (!IsDoubleFour(i-1, y)) && (!IsDoubleThree(i-1, y)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		case 2:		// vertial direction
			i = y;
			while (i > 0)
			{
				if (cBoard[x+1][i] == c)
				{
					i--;
					continue;
				}
				else if (cBoard[x+1][i] == C_EMPTY)
				{
					if ((IsOpenFour(x, i-1, nColor, nDir) == 1) && (!IsDoubleFour(x, i-1)) && (!IsDoubleThree(x, i-1)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = y+2;
			while (i < (f_boardsize+1))
			{
				if (cBoard[x+1][i] == c)
				{
					i++;
					continue;
				}
				else if (cBoard[x+1][i] == C_EMPTY)
				{
					if ((IsOpenFour(x, i-1, nColor, nDir) == 1) && (!IsDoubleFour(x, i-1)) && (!IsDoubleThree(x, i-1)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		case 3:		// diagonal direction - '/'
			i = x;
			j = y;
			while ((i > 0) && (j > 0))
			{
				if (cBoard[i][j] == c)
				{
					i--;
					j--;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if ((IsOpenFour(i-1, j-1, nColor, nDir) == 1) && (!IsDoubleFour(i-1, j-1)) && (!IsDoubleThree(i-1, j-1)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = x+2;
			j = y+2;
			while ((i < (f_boardsize+1)) && (j < (f_boardsize+1)))
			{
				if (cBoard[i][j] == c)
				{
					i++;
					j++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if ((IsOpenFour(i-1, j-1, nColor, nDir) == 1) && (!IsDoubleFour(i-1, j-1)) && (!IsDoubleThree(i-1, j-1)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		case 4:		// diagonal direction - '\'
			i = x;
			j = y+2;
			while ((i > 0) && (j < (f_boardsize+1)))
			{
				if (cBoard[i][j] == c)
				{
					i--;
					j++;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if ((IsOpenFour(i-1, j-1, nColor, nDir) == 1) && (!IsDoubleFour(i-1, j-1)) && (!IsDoubleThree(i-1, j-1)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else 
						break;
				}
				else
					break;
			}
			i = x+2;
			j = y;
			while ((i < (f_boardsize+1)) && (j > 0))
			{
				if (cBoard[i][j] == c)
				{
					i++;
					j--;
					continue;
				}
				else if (cBoard[i][j] == C_EMPTY)
				{
					if ((IsOpenFour(i-1, j-1, nColor, nDir) == 1) && (!IsDoubleFour(i-1, j-1)) && (!IsDoubleThree(i-1, j-1)))
					{
						SetStone(x, y, C_EMPTY);
						return true;
					}
					else
						break;
				}
				else
					break;
			}
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		default:
			SetStone(x, y, C_EMPTY);
			return false;
			break;
		}
	}
}

bool CForbiddenPointFinder::IsDoubleThree(int x, int y)
{
	if (cBoard[x+1][y+1] != C_EMPTY)
		return false;

	if (IsFive(x, y, C_BLACK))	// five?
		return false;
	
	int nThree = 0;
	for (int i=1; i<=4; i++)
	{
		if (IsOpenThree(x, y, C_BLACK, i))
			nThree++;
	}

	if (nThree >= 2)
		return true;
	else
		return false;
}

bool CForbiddenPointFinder::isForbidden(int x, int y)
{
	int nearbyBlack = 0;//检查周围16个子
	x++; y++;
	if (cBoard[x][y] != C_EMPTY)return false;
	if (x >= 2 && x <= f_boardsize - 1 && y >= 2 && y <= f_boardsize - 1)
	{
		if (cBoard[x + 2][y + 2] == C_BLACK)nearbyBlack++;
		if (cBoard[x + 2][y  ] == C_BLACK)nearbyBlack++;
		if (cBoard[x + 2][y -2 ] == C_BLACK)nearbyBlack++;
		if (cBoard[x ][y + 2] == C_BLACK)nearbyBlack++;
		if (cBoard[x  ][y - 2] == C_BLACK)nearbyBlack++;
		if (cBoard[x - 2][y - 2] == C_BLACK)nearbyBlack++;
		if (cBoard[x-2][y ] == C_BLACK)nearbyBlack++;
		if (cBoard[x-2][y + 2] == C_BLACK)nearbyBlack++;
		if (cBoard[x + 1][y - 1] == C_BLACK)nearbyBlack++;
		if (cBoard[x + 1][y ] == C_BLACK)nearbyBlack++;
		if (cBoard[x + 1][y + 1] == C_BLACK)nearbyBlack++;
		if (cBoard[x ][y - 1] == C_BLACK)nearbyBlack++;
		if (cBoard[x][y + 1] == C_BLACK)nearbyBlack++;
		if (cBoard[x - 1][y - 1] == C_BLACK)nearbyBlack++;
		if (cBoard[x - 1][y] == C_BLACK)nearbyBlack++;
		if (cBoard[x - 1][y + 1] == C_BLACK)nearbyBlack++;
	}
	else
	{
		for (int i = std::max(x - 2, 1); i <= std::min(x + 2, f_boardsize); i++)
			for (int j = std::max(y - 2, 1); j <= std::min(y + 2, f_boardsize); j++)
			{
				int xd = i - x;
				int yd = j - y;
				xd = xd > 0 ? xd : -xd;
				yd = yd > 0 ? yd : -yd;
				if(((xd+yd)!=3)&& cBoard[i][j] == C_BLACK)nearbyBlack++;
			}
	}
	if (nearbyBlack < 2)return false;
	x--; y--;
	return isForbiddenNoNearbyCheck(x, y);


}

bool CForbiddenPointFinder::isForbiddenNoNearbyCheck(int x, int y)
{
	if (IsDoubleThree(x, y) || IsDoubleFour(x, y) || IsOverline(x, y))
	{
		return true;
	}
	return false;
}
/*
void CForbiddenPointFinder::FindForbiddenPoints()
{
	nForbiddenPoints = 0;
	for (int i=0; i<f_boardsize; i++)
	{
		for (int j=0; j<f_boardsize; j++)
		{
			if (cBoard[i+1][j+1] != C_EMPTY)
				continue;
			else
			{
				if (IsOverline(i, j) || IsDoubleFour(i, j) || IsDoubleThree(i, j))
				{
					ptForbidden[nForbiddenPoints].x = i;
					ptForbidden[nForbiddenPoints].y = j;
					nForbiddenPoints++;
				}
			}
		}
	}
}*/