
#include <fstream>
#include <sstream>

#include "config.hpp"
#include "gecko/gecko.hpp"
#include "gui/text.hpp"

static const char *g_whitespaces = " \f\n\r\t\v";
static const int g_floatPrecision = 10;

const std::string Config::emptyString;

Config::Config(void) :
	m_loaded(false), m_changed(false), m_domains(), m_filename(), m_iter() 
{
}

static std::string trimEnd(std::string line)
{
	std::string::size_type i = line.find_last_not_of(g_whitespaces);
	if (i == std::string::npos) line.clear();
	else line.resize(i + 1);
	return line;
}

static std::string trim(std::string line)
{
	std::string::size_type i = line.find_last_not_of(g_whitespaces);
	if (i == std::string::npos)
	{
		line.clear();
		return line;
	}
	else
		line.resize(i + 1);
	i = line.find_first_not_of(g_whitespaces);
	if (i > 0)
		line.erase(0, i);
	return line;
}

static std::string unescNewlines(const std::string &text)
{
	std::string s;
	bool escaping = false;

	s.reserve(text.size());
	for (std::string::size_type i = 0; i < text.size(); ++i)
	{
		if (escaping)
		{
			switch (text[i])
			{
				case 'n':
					s.push_back('\n');
					break;
				default:
					s.push_back(text[i]);
			}
			escaping = false;
		}
		else if (text[i] == '\\')
			escaping = true;
		else
			s.push_back(text[i]);
	}
	return s;
}

static std::string escNewlines(const std::string &text)
{
	std::string s;

	s.reserve(text.size());
	for (std::string::size_type i = 0; i < text.size(); ++i)
	{
		switch (text[i])
		{
			case '\n':
				s.push_back('\\');
				s.push_back('n');
				break;
			case '\\':
				s.push_back('\\');
				s.push_back('\\');
				break;
			default:
				s.push_back(text[i]);
		}
	}
	return s;
}

bool Config::hasDomain(const std::string &domain) const
{
	return m_domains.find(domain) != m_domains.end();
}

void Config::copyDomain(const std::string &dst, const std::string &src)
{
	m_domains[upperCase(dst)] = m_domains[upperCase(src)];
}

const std::string &Config::firstDomain(void)
{
	m_iter = m_domains.begin();
	if (m_iter == m_domains.end())
		return Config::emptyString;
	return m_iter->first;
}

const std::string &Config::nextDomain(void)
{
	++m_iter;
	if (m_iter == m_domains.end())
		return Config::emptyString;
	return m_iter->first;
}

const std::string &Config::nextDomain(const std::string &start) const
{
	Config::DomainMap::const_iterator i;
	Config::DomainMap::const_iterator j;
	if (m_domains.empty())
		return Config::emptyString;
	i = m_domains.find(start);
	if (i == m_domains.end())
		return m_domains.begin()->first;
	j = i;
	++j;
	return j != m_domains.end() ? j->first : i->first;
}

const std::string &Config::prevDomain(const std::string &start) const
{
	Config::DomainMap::const_iterator i;
	if (m_domains.empty())
		return Config::emptyString;
	i = m_domains.find(start);
	if (i == m_domains.end() || i == m_domains.begin())
		return m_domains.begin()->first;
	--i;
	return i->first;
}

bool Config::load(const char *filename)
{
	if(m_loaded)
		return true;
	//if (m_loaded && m_changed) save();
	
	std::ifstream file(filename, std::ios::in | std::ios::binary);
	std::string line;
	std::string domain("");

	m_changed = false;
	m_loaded = false;
	m_filename = filename;
	u32 n = 0;
	if (!file.is_open()) return m_loaded;
	m_domains.clear();
	while (file.good())
	{
		line.clear();
		std::getline(file, line, '\n');
		++n;
		if (!file.bad() && !file.fail())
		{
			line = trimEnd(line);
			if (line.empty() || line[0] == '#' || line[0] == '\0') continue;
			if (line[0] == '[')
			{
				std::string::size_type i = line.find_first_of(']');
				if (i != std::string::npos && i > 1)
				{
					domain = upperCase(line.substr(1, i - 1));
					if (m_domains.find(domain) != m_domains.end())
						domain.clear();
				}
			}
			else
				if (!domain.empty())
				{
					std::string::size_type i = line.find_first_of('=');
					if (i != std::string::npos && i > 0)
						m_domains[domain][lowerCase(trim(line.substr(0, i)))] = unescNewlines(trim(line.substr(i + 1)));
				}
		}
	}
	file.close(); /* not sure if needed */
	m_loaded = true;
	return m_loaded;
}

void Config::unload(void)
{
	m_loaded = false;
	m_changed = false;
	m_filename = emptyString;
	m_domains.clear();
	m_groupCustomTitles.clear();
}

void Config::save(bool unload)
{
	if (m_changed)
	{
		//gprintf("changed:%d\n",m_changed);
		std::ofstream file(m_filename.c_str(), std::ios::out | std::ios::binary);
		for (Config::DomainMap::iterator k = m_domains.begin(); k != m_domains.end(); ++k)
		{
			Config::KeyMap *m = &k->second;
			file << '\n' << '[' << k->first << ']' << '\n';
			for (Config::KeyMap::iterator l = m->begin(); l != m->end(); ++l)
				file << l->first << '=' << escNewlines(l->second) << '\n';
		}
		file.close(); /* not sure if needed */
		m_changed = false;
	}
	if(unload) this->unload();
}

bool Config::has(const std::string &domain, const std::string &key) const
{
	if (domain.empty() || key.empty()) return false;
	DomainMap::const_iterator i = m_domains.find(upperCase(domain));
	if (i == m_domains.end()) return false;
	return i->second.find(lowerCase(key)) != i->second.end();
}

void Config::groupCustomTitles(void)
{
	for (Config::DomainMap::iterator k = m_domains.begin(); k != m_domains.end(); ++k)
	{
		std::string uc_domain(upperCase(k->first));
		std::istringstream f(uc_domain);
		std::string s;
		while (getline(f, s, ','))
			m_groupCustomTitles[s] = uc_domain;
	}
}

void Config::setWString(const std::string &domain, const std::string &key, const wstringEx &val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setWString %s\n", val.toUTF8().c_str());
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = val.toUTF8();
}

void Config::setString(const std::string &domain, const std::string &key, const std::string &val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setString %s\n", val.c_str());
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = val;
}

void Config::setBool(const std::string &domain, const std::string &key, bool val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setBool %d\n", val);
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = val ? "yes" : "no";
}

void Config::remove(const std::string &domain, const std::string &key)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("remove %s\n", key.c_str());
	m_changed = true;
	m_domains[upperCase(domain)].erase(lowerCase(key));
}

void Config::setOptBool(const std::string &domain, const std::string &key, int val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setOptBool %d\n", val);
	m_changed = true;
	switch (val)
	{
		case 0:
			m_domains[upperCase(domain)][lowerCase(key)] = "no";
			break;
		case 1:
			m_domains[upperCase(domain)][lowerCase(key)] = "yes";
			break;
		default:
			m_domains[upperCase(domain)][lowerCase(key)] = "default";
	}
}

void Config::setInt(const std::string &domain, const std::string &key, int val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setInt %i\n", val);
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = sfmt("%i", val);
}

void Config::setUInt(const std::string &domain, const std::string &key, unsigned int val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setUInt %u\n", val);
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = sfmt("%u", val);
}

void Config::setFloat(const std::string &domain, const std::string &key, float val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setFloat %f\n", val);
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = sfmt("%.*g", g_floatPrecision, val);
}

void Config::setVector3D(const std::string &domain, const std::string &key, const Vector3D &val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setVector3D\n");
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = sfmt("%.*g, %.*g, %.*g", g_floatPrecision, val.x, g_floatPrecision, val.y, g_floatPrecision, val.z);
}

void Config::setColor(const std::string &domain, const std::string &key, const CColor &val)
{
	if (domain.empty() || key.empty()) return;
	//gprintf("setColor\n");
	m_changed = true;
	m_domains[upperCase(domain)][lowerCase(key)] = sfmt("#%.2X%.2X%.2X%.2X", val.r, val.g, val.b, val.a);
}

wstringEx Config::getWString(const std::string &domain, const std::string &key, const wstringEx &defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty())
	{
		data = defVal.toUTF8();
		//gprintf("getWString %s\n", defVal.toUTF8().c_str());
		m_changed = true;
		return defVal;
	}
	wstringEx ws;
	ws.fromUTF8(data.c_str());
	return ws;
}

std::string Config::getString(const std::string &domain, const std::string &key, const std::string &defVal)
{
	if(domain.empty() || key.empty())
		return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if(data.empty())
	{
		data = defVal;
		//gprintf("setString %s\n", defVal.c_str());
		m_changed = true;
	}
	return data;
}

std::string Config::getStringCustomTitles(const std::string &domain, const std::string &key, const std::string &defVal)
{
	if(domain.empty() || key.empty())
		return defVal;
	KeyMap::iterator i = m_groupCustomTitles.find(upperCase(domain));
	if (i == m_groupCustomTitles.end()) return defVal;
	std::string &data = m_domains[i->second][lowerCase(key)];
	if(data.empty())
	{
		data = defVal;
		//gprintf("setString %s\n", defVal.c_str());
		m_changed = true;
	}
	return data;
}

std::vector<std::string> Config::getStrings(const std::string &domain, const std::string &key, char seperator, const std::string &defVal)
{
	std::vector<std::string> retval;

	if(domain.empty() || key.empty())
	{
		if(!defVal.empty())
			retval.push_back(defVal);
		return retval;
	}

	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if(data.empty())
	{
		if(!defVal.empty())
			retval.push_back(defVal);
		return retval;
	}
	// Parse the string into different substrings

	// skip delimiters at beginning.
	std::string::size_type lastPos = data.find_first_not_of(seperator, 0);

	// find first "non-delimiter".
	std::string::size_type pos = data.find_first_of(seperator, lastPos);

	// no seperator found, return data
	if(pos == std::string::npos)
	{
		retval.push_back(data);
		return retval;
	}

	while(std::string::npos != pos || std::string::npos != lastPos)
	{
		// found a token, add it to the vector.
		retval.push_back(data.substr(lastPos, pos - lastPos));
	
		// skip delimiters.  Note the "not_of"
		lastPos = data.find_first_not_of(seperator, pos);
	
		// find next "non-delimiter"
		pos = data.find_first_of(seperator, lastPos);
	}

	return retval;
}

bool Config::getBool(const std::string &domain, const std::string &key, bool defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty())
	{
		data = defVal ? "yes" : "no";
		//gprintf("getBool %d\n", defVal);
		m_changed = true;
		return defVal;
	}
	std::string s(lowerCase(trim(data)));
	if (s == "yes" || s == "true" || s == "y" || s == "1")
		return true;
	return false;
}

bool Config::testOptBool(const std::string &domain, const std::string &key, bool defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	KeyMap &km = m_domains[upperCase(domain)];
	KeyMap::iterator i = km.find(lowerCase(key));
	if (i == km.end()) return defVal;
	std::string s(lowerCase(trim(i->second)));
	if (s == "yes" || s == "true" || s == "y" || s == "1")
		return true;
	if (s == "no" || s == "false" || s == "n" || s == "0")
		return false;
	return defVal;
}

int Config::getOptBool(const std::string &domain, const std::string &key, int defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty())
	{
		switch (defVal)
		{
			case 0:
				data = "no";
				break;
			case 1:
				data = "yes";
				break;
			default:
				data = "default";
		}
		//gprintf("getOptBool %s\n", data.c_str());
		m_changed = true;
		return defVal;
	}
	std::string s(lowerCase(trim(data)));
	if (s == "yes" || s == "true" || s == "y" || s == "1")
		return 1;
	if (s == "no" || s == "false" || s == "n" || s == "0")
		return 0;
	return 2;
}

int Config::getInt(const std::string &domain, const std::string &key, int defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty())
	{
		data = sfmt("%i", defVal);
		//gprintf("getInt %i\n", defVal);
		m_changed = true;
		return defVal;
	}
	return strtol(data.c_str(), 0, 10);
}

/* this returns true only if there's something after the '=' and value is set to the integer value */
bool Config::getInt(const std::string &domain, const std::string &key, int *value)
{
	if (domain.empty() || key.empty()) return false;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty()) return false;
	*value = strtol(data.c_str(), 0, 10);
	return true;
}

unsigned int Config::getUInt(const std::string &domain, const std::string &key, unsigned int defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty())
	{
		data = sfmt("%u", defVal);
		//gprintf("getUInt %u\n", defVal);
		m_changed = true;
		return defVal;
	}
	return strtoul(data.c_str(), 0, 10);
}

float Config::getFloat(const std::string &domain, const std::string &key, float defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	if (data.empty())
	{
		data = sfmt("%.*g", g_floatPrecision, defVal);
		//gprintf("getFloat %s\n", data.c_str());
		m_changed = true;
		return defVal;
	}
	return strtod(data.c_str(), 0);
}

Vector3D Config::getVector3D(const std::string &domain, const std::string &key, const Vector3D &defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	std::string::size_type i;
	std::string::size_type j = std::string::npos;
	i = data.find_first_of(',');
	if (i != std::string::npos) j = data.find_first_of(',', i + 1);
	if (j == std::string::npos)
	{
		data = sfmt("%.*g, %.*g, %.*g", g_floatPrecision, defVal.x, g_floatPrecision, defVal.y, g_floatPrecision, defVal.z);
		//gprintf("getVector3D\n");
		m_changed = true;
		return defVal;
	}
	return Vector3D(strtod(data.substr(0, i).c_str(), 0), strtod(data.substr(i + 1, j - i - 1).c_str(), 0), strtod(data.substr(j + 1).c_str(), 0));
}

CColor Config::getColor(const std::string &domain, const std::string &key, const CColor &defVal)
{
	if (domain.empty() || key.empty()) return defVal;
	std::string &data = m_domains[upperCase(domain)][lowerCase(key)];
	std::string text(upperCase(trim(data)));
	u32 i = (u32)text.find_first_of('#');
	if (i != std::string::npos)
	{
		text.erase(0, i + 1);
		i = (u32)text.find_first_not_of("0123456789ABCDEF");
		if ((i != std::string::npos && i >= 6) || (i == std::string::npos && text.size() >= 6))
		{
			u32 n = ((i != std::string::npos && i >= 8) || (i == std::string::npos && text.size() >= 8)) ? 8 : 6;
			for (i = 0; i < n; ++i)
				if (text[i] <= '9')
					text[i] -= '0';
				else
					text[i] -= 'A' - 10;
			CColor c(text[0] * 0x10 + text[1], text[2] * 0x10 + text[3], text[4] * 0x10 + text[5], 1.f);
			if (n == 8)
				c.a = text[6] * 0x10 + text[7];
			return c;
		}
	}
	data = sfmt("#%.2X%.2X%.2X%.2X", defVal.r, defVal.g, defVal.b, defVal.a);
	//gprintf("getColor\n");
	m_changed = true;
	return defVal;
}