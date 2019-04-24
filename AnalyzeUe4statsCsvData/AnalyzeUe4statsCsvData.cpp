#define _CRT_SECURE_NO_WARNINGS
#include "csv.h"
#include <map>
#include <iostream>
#include <fstream>

namespace
{
	std::vector<std::string> split(const std::string& s, char seperator)
	{
		std::vector<std::string> output;

		std::string::size_type prev_pos = 0, pos = 0;

		while ((pos = s.find(seperator, pos)) != std::string::npos)
		{
			std::string substring(s.substr(prev_pos, pos - prev_pos));

			output.push_back(substring);

			prev_pos = ++pos;
		}

		output.push_back(s.substr(prev_pos, pos - prev_pos)); // Last word

		return output;
	}

	struct StatData
	{
		int count;
		double value;
		StatData() : count(0), value(0) {}
	};

	class StatFile
	{
	public:
		bool LoadFile(const std::string& filename)
		{
			io::CSVReader<3> in(filename);
			int frame;
			std::string name;
			double value;

			in.read_header(io::ignore_extra_column, "Frame", "Name", "Value");
			while (in.read_row(frame, name, value))
			{
				name = name.substr(5); // remove "STAT_"
				++m_data[name].count;
				m_data[name].value += value;
			}

			return true;
		}

		const StatData* GetData(const std::string& name) const
		{
			auto iter = m_data.find(name);
			if (iter != m_data.end())
				return &iter->second;
			return nullptr;
		}
	private:
		std::map<std::string, StatData> m_data;
	};

	class CombinedStatFile
	{
	public:
		bool ParseRowName(const std::string& arg)
		{
			std::vector<std::string> splitName = split(arg, '+');
			for (size_t i = 0; i < splitName.size(); i++)
			{
				if (splitName[i].rfind("_WorkerThread") == std::string::npos)
					m_rowName.push_back(splitName[i].substr(5)); // remove "STAT_"
			}
			return true;
		}

		bool LoadFile(const std::string& filename)
		{
			std::map<std::string, StatData> fileData;

			io::CSVReader<3> in(filename);
			int frame;
			std::string name;
			double value;

			in.read_header(io::ignore_extra_column, "Frame", "Name", "Value");
			while (in.read_row(frame, name, value))
			{
				name = name.substr(5); // remove "STAT_"
				++fileData[name].count;
				fileData[name].value += value;
			}

			std::vector<float> col;
			col.resize(m_rowName.size(), 0);
			for (size_t i = 0; i < m_rowName.size(); i++)
			{
				auto iter = fileData.find(m_rowName[i]);
				auto iter2 = fileData.find(m_rowName[i] + "_WorkerThread");
				if (iter != fileData.end())
					col[i] += (float)iter->second.value / iter->second.count;
				if (iter2 != fileData.end())
					col[i] += (float)iter2->second.value / iter2->second.count;
			}

			m_data.push_back(col);

			return true;
		}

		bool AddFile(const StatFile* statFile)
		{
			std::vector<float> col;
			col.resize(m_rowName.size(), 0);
			for (size_t i = 0; i < m_rowName.size(); i++)
			{
				const StatData* data = statFile->GetData(m_rowName[i]);
				const StatData* data_WorkerThread = statFile->GetData(m_rowName[i] + "_WorkerThread");
				if (data)
					col[i] += (float)data->value / data->count;
				if (data_WorkerThread)
					col[i] += (float)data_WorkerThread->value / data_WorkerThread->count;
			}
			m_data.push_back(col);
			return true;
		}

		bool WriteFile(const std::string& filename)
		{
			std::ofstream out(filename);

			for (size_t i = 0; i < m_rowName.size(); i++)
			{
				out << m_rowName[i] << ',';
				for (size_t j = 0; j < m_data.size(); j++)
					out << m_data[j][i] << ',';
				out << '\n';
			}

			out.close();

			return true;
		}

	private:
		std::vector<std::string> m_rowName;
		std::vector<std::vector<float>> m_data;
	};
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		printf("argc error!\n");
		return 0;
	}

	CombinedStatFile combinedFile;

	combinedFile.ParseRowName(argv[1]);

	for (int i = 2; i < argc; i++)
		combinedFile.LoadFile(argv[i]);

	combinedFile.WriteFile("analyze.csv");
}
