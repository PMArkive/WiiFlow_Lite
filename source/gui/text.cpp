#include <algorithm>
#include "text.hpp"
#include "memory/mem2.hpp"
#include "fileOps/fileOps.h"

static char general_buffer[MAX_MSG_SIZE * 2];
string sfmt(const char *format, ...)
{
	va_list va;
	va_start(va, format);
	size_t len = vsnprintf(general_buffer, MAX_MSG_SIZE - 1, format, va);
	general_buffer[MAX_MSG_SIZE - 1] = '\0';
	va_end(va);
	return string(general_buffer, len);
}

static inline bool fmtCount(const wstringEx &format, int &i, int &s)
{
	int state = 0;

	i = 0;
	s = 0;
	for (u32 k = 0; k < format.size(); ++k)
	{
		if (state == 0)
		{
			if (format[k] == L'%')
				state = 1;
		}
		else if (state == 1)
		{
			switch (format[k])
			{
				case L'%':
					state = 0;
					break;
				case L'i':
				case L'd':
					state = 0;
					++i;
					break;
				case L's':
					state = 0;
					++s;
					break;
				default:
					return false;
			}
		}
	}
	return true;
}

// Only handles the cases i need for translations : plain %i and %s
bool checkFmt(const wstringEx &ref, const wstringEx &format)
{
	int s;
	int i;
	int refs;
	int refi;
	if (!fmtCount(ref, refi, refs))
		return false;
	if (!fmtCount(format, i, s))
		return false;
	return i == refi && s == refs;
}

wstringEx wfmt(const wstringEx &format, ...)
{
	va_list va;
	va_start(va, format);
	vsnprintf(general_buffer, MAX_MSG_SIZE - 1, format.toUTF8().c_str(), va);
	general_buffer[MAX_MSG_SIZE - 1] = '\0';
	va_end(va);
	wstringEx wide_buffer;
	wide_buffer.fromUTF8(general_buffer);
	return wide_buffer;
}

string vectorToString(const vector<string> &vect, string sep)
{
	string s;
	for (u32 i = 0; i < vect.size(); ++i)
	{
		if (i > 0)
			s.append(sep);
		s.append(vect[i]);
	}
	return s;
}

wstringEx vectorToString(const vector<wstringEx> &vect, char sep)
{
	wstringEx s;
	for (u32 i = 0; i < vect.size(); ++i)
	{
		if (i > 0)
			s.push_back(sep);
		s.append(vect[i]);
	}
	return s;
}

vector<string> stringToVector(const string &text, char sep)
{
	vector<string> v;
	if (text.empty()) return v;
	u32 count = 1;
	for (u32 i = 0; i < text.size(); ++i)
		if (text[i] == sep)
			++count;
	v.reserve(count);
	string::size_type off = 0;
	string::size_type i = 0;
	do
	{
		i = text.find_first_of(sep, off);
		if (i != string::npos)
		{
			string ws(text.substr(off, i - off));
			v.push_back(ws);
			off = i + 1;
		}
		else
			v.push_back(text.substr(off));
	} while (i != string::npos);
	return v;
}

vector<wstringEx> stringToVector(const wstringEx &text, char sep)
{
	vector<wstringEx> v;
	if (text.empty()) return v;
	u32 count = 1;
	for (u32 i = 0; i < text.size(); ++i)
		if (text[i] == sep)
			++count;
	v.reserve(count);
	wstringEx::size_type off = 0;
	wstringEx::size_type i = 0;
	do
	{
		i = text.find_first_of(sep, off);
		if (i != wstringEx::npos)
		{
			wstringEx ws(text.substr(off, i - off));
			v.push_back(ws);
			off = i + 1;
		}
		else
			v.push_back(text.substr(off));
	} while (i != wstringEx::npos);
	return v;
}

void SFont::ClearData(void)
{
	if(font != NULL)
		delete font;
	font = NULL;
	if(data != NULL)
		free(data);
	data = NULL;
	dataSize = 0;
}

bool SFont::fromBuffer(const u8 *buffer, const u32 bufferSize, u32 size, u32 lspacing, u32 w, u32 idx, const char *fontname)
{
	if(buffer == NULL)
		return false;
	fSize = std::min(std::max(6u, size), 1000u);
	lineSpacing = std::min(std::max(6u, lspacing), 1000u);
	weight = std::min(w, 32u);
	index = idx;// currently not used

	if(data != NULL)
		free(data);
	data = (u8*)MEM2_alloc(bufferSize);
	if(data == NULL) return false;
	dataSize = bufferSize;

	memcpy(data, buffer, bufferSize);
	DCFlushRange(data, dataSize);

	strncpy(name, fontname, 127);
	font = new FreeTypeGX();
	font->loadFont(data, dataSize, weight, true);
	return true;
}

bool SFont::fromFile(const char *path, u32 size, u32 lspacing, u32 w, u32 idx, const char *fontname)
{
	fSize = std::min(std::max(6u, size), 1000u);
	weight = std::min(w, 32u);
	index = idx;// currently not used

	lineSpacing = std::min(std::max(6u, lspacing), 1000u);

	if(data != NULL)
		free(data);
	data = fsop_ReadFile(path, &dataSize);
	if(data == NULL)
		return false;

	DCFlushRange(data, dataSize);

	strncpy(name, fontname, 127);
	font = new FreeTypeGX();
	font->loadFont(data, dataSize, weight, false);
	return true;
}

static const wchar_t *g_whitespaces = L" \f\n\r\t\v";// notice the first character is a space
void CText::setText(const SFont &font, const wstringEx &t)
{
	SWord w;
	m_lines.clear();
	if(font.font == NULL)
		return;
	m_font = font;
	firstLine = 0;
	// Don't care about performance
	vector<wstringEx> lines = stringToVector(t, L'\n');
	m_lines.reserve(lines.size());
	// 
	for (u32 k = 0; k < lines.size(); ++k)
	{
		wstringEx &l = lines[k];
		m_lines.push_back(CLine());
		m_lines.back().reserve(32);
		wstringEx::size_type i = l.find_first_not_of(g_whitespaces);
		wstringEx::size_type j;
		while (i != wstringEx::npos)
		{
			j = l.find_first_of(g_whitespaces, i);// find a space or end of line character
			if (j != wstringEx::npos && j > i)
			{
				w.text.assign(l, i, j - i);
				m_lines.back().push_back(w);
				i = l.find_first_not_of(g_whitespaces, j);
			}
			else if (j == wstringEx::npos)// if end of line
			{
				w.text.assign(l, i, l.size() - i);
				m_lines.back().push_back(w);
				i = wstringEx::npos;// or could just break;
			}
		}
	}
}

void CText::setText(const SFont &font, const wstringEx &t, u32 startline)
{
	SWord w;
	totalHeight = 0;

	m_lines.clear();
	if(font.font != NULL)
		m_font = font;
	if(m_font.font == NULL)
		return;

	firstLine = startline;
	// Don't care about performance
	vector<wstringEx> lines = stringToVector(t, L'\n');
	m_lines.reserve(lines.size());
	// 
	for (u32 k = 0; k < lines.size(); ++k)
	{
		wstringEx &l = lines[k];
		m_lines.push_back(CLine());
		m_lines.back().reserve(32);
		wstringEx::size_type i = l.find_first_not_of(g_whitespaces);
		wstringEx::size_type j;
		while (i != wstringEx::npos)
		{
			j = l.find_first_of(g_whitespaces, i);
			if (j != wstringEx::npos && j > i)
			{
				w.text.assign(l, i, j - i);
				m_lines.back().push_back(w);
				i = l.find_first_not_of(g_whitespaces, j);
			}
			else if (j == wstringEx::npos)
			{
				w.text.assign(l, i, l.size() - i);
				m_lines.back().push_back(w);
				i = wstringEx::npos;
			}
		}
	}
}

void CText::setFrame(float width, u16 style, bool ignoreNewlines, bool instant)
{
	if(m_font.font == NULL)
		return;

	float shift;
	totalHeight = 0;
	float space = m_font.font->getWidth(L" ", m_font.fSize);//
	float posX = 0.f;
	float posY = 0.f;
	u32 lineBeg = 0;

	if(firstLine > m_lines.size()) firstLine = 0;

	for (u32 k = firstLine; k < m_lines.size(); ++k)
	{
		CLine &words = m_lines[k];
		if (words.empty())
		{
			posY += (float)m_font.lineSpacing;
			continue;
		}

		for (u32 i = 0; i < words.size(); ++i)
		{
			float wordWidth = m_font.font->getWidth(words[i].text.c_str(), m_font.fSize);//
			if (posX == 0.f || posX + (float)wordWidth + space * 2 <= width)
			{
				words[i].targetPos = Vector3D(posX, posY, 0.f);
				posX += wordWidth + space;
			}
			else
			{
				posY += (float)m_font.lineSpacing;
				words[i].targetPos = Vector3D(0.f, posY, 0.f);
				if ((style & (FTGX_JUSTIFY_CENTER | FTGX_JUSTIFY_RIGHT)) != 0)
				{
					posX -= space;
					shift = (style & FTGX_JUSTIFY_CENTER) != 0 ? -posX * 0.5f : -posX;
					for (u32 j = lineBeg; j < i; ++j)
						words[j].targetPos.x += shift;
				}
				posX = wordWidth + space;
				lineBeg = i;
			}
		}
		// Quick patch for newline support
		if (!ignoreNewlines && k + 1 < m_lines.size())
			posX = 9999999.f;
	}
	totalHeight = posY + m_font.lineSpacing;
	
	if ((style & (FTGX_JUSTIFY_CENTER | FTGX_JUSTIFY_RIGHT)) != 0)
	{
		posX -= space;
		shift = (style & FTGX_JUSTIFY_CENTER) != 0 ? -posX * 0.5f : -posX;
		for (u32 k = firstLine; k < m_lines.size(); ++k)
			for (u32 j = lineBeg; j < m_lines[k].size(); ++j)
				m_lines[k][j].targetPos.x += shift;
	}
	if ((style & (FTGX_ALIGN_MIDDLE | FTGX_ALIGN_BOTTOM)) != 0)
	{
		posY += (float)m_font.lineSpacing;
		shift = (style & FTGX_ALIGN_MIDDLE) != 0 ? -posY * 0.5f : -posY;
		for (u32 k = firstLine; k < m_lines.size(); ++k)
			for (u32 j = 0; j < m_lines[k].size(); ++j)
				m_lines[k][j].targetPos.y += shift;
	}
	if (instant)
		for (u32 k = firstLine; k < m_lines.size(); ++k)
			for (u32 i = 0; i < m_lines[k].size(); ++i)
				m_lines[k][i].pos = m_lines[k][i].targetPos;
}

void CText::setColor(const CColor &c)
{
	m_color = c;
}

/* moves each word of each line of text from current pos to targetPos */
void CText::tick(void)
{
	for (u32 k = 0; k < m_lines.size(); ++k)// lines of text
		for (u32 i = 0; i < m_lines[k].size(); ++i)// number of words per line[k]
			m_lines[k][i].pos += (m_lines[k][i].targetPos - m_lines[k][i].pos) * 0.05f;
}

void CText::draw(void)
{
	if(m_font.font == NULL)
		return;

	for (u32 k = firstLine; k < m_lines.size(); ++k)
		for (u32 i = 0; i < m_lines[k].size(); ++i)
		{
			m_font.font->setX(m_lines[k][i].pos.x);
			m_font.font->setY(m_lines[k][i].pos.y);
			m_font.font->drawText(0, m_font.lineSpacing, m_lines[k][i].text.c_str(), m_font.fSize, m_color);
		}
}

int CText::getTotalHeight(void)
{
	return totalHeight;
}

string upperCase(string text)
{
	char c;
	for (string::size_type i = 0; i < text.size(); ++i)
	{
		c = text[i];
		if (c >= 'a' && c <= 'z')
			text[i] = c & 0xDF;
	}
	return text;
}


string lowerCase(string text)
{
	char c;
	for (string::size_type i = 0; i < text.size(); ++i)
	{
		c = text[i];
		if (c >= 'A' && c <= 'Z')
			text[i] = c | 0x20;
	}
	return text;
}

// trim from start
/*string ltrim(string s)
{
	s.erase(s.begin(), find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(isspace))));
	return s;
}
*/
// trim from end
/*string rtrim(string s)
{
	s.erase(find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(isspace))).base(), s.end());
	return s;
}*/

bool wchar_cmp(const wchar_t *first, const wchar_t *second, u32 first_len, u32 second_len)
{
	u32 i = 0;
	while((i < first_len) && (i < second_len))
	{
		if(tolower(first[i]) < tolower(second[i]))
			return true;
		else if(tolower(first[i]) > tolower(second[i]))
			return false;
		++i;
	}
	return first_len < second_len;
}

bool char_cmp(const char *first, const char *second, u32 first_len, u32 second_len)
{
	u32 i = 0;
	while((i < first_len) && (i < second_len))
	{
		if(tolower(first[i]) < tolower(second[i]))
			return true;
		else if(tolower(first[i]) > tolower(second[i]))
			return false;
		++i;
	}
	return first_len < second_len;
}
