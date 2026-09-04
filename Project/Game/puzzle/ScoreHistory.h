#pragma once
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

struct ScoreRecord{
	int64_t score = 0;
	int64_t cells = 0;
	std::string date;
};

// 保存は追記式。過去の記録を上書きせず、表示用には上位10件を保持する。
class ScoreHistory{
public:
	bool Load(const std::filesystem::path& path){
		records_.clear();
		std::error_code ec;
		const bool exists = std::filesystem::exists(path,ec);
		if(ec){ return false; }
		if(!exists){ return true; }
		std::ifstream in(path);
		if(!in){ return false; }
		bool valid = true;
		std::string line;
		while(std::getline(in,line)){
			if(line.empty()){ continue; }
			std::istringstream row(line);
			ScoreRecord record;
			if(!(row >> record.score >> record.cells >> std::quoted(record.date)) ||
				record.score < 0 || record.cells < 0 || record.date.size() != 19){
				valid = false;
				continue;
			}
			row >> std::ws;
			if(!row.eof()){ valid = false; continue; }
			Add(record);
		}
		return valid && !in.bad();
	}
	void Add(const ScoreRecord& record){
		records_.push_back(record);
		// 同点では先に登録された記録を優先する。
		std::stable_sort(records_.begin(),records_.end(),[](const auto& a,const auto& b){ return a.score > b.score; });
		if(records_.size() > 10){ records_.resize(10); }
	}
	static bool Append(const std::filesystem::path& path,const ScoreRecord& record){
		std::error_code ec;
		if(!path.parent_path().empty()){ std::filesystem::create_directories(path.parent_path(),ec); }
		if(ec){ return false; }
		std::ofstream out(path,std::ios::app);
		if(!out){ return false; }
		out << record.score << ' ' << record.cells << ' ' << std::quoted(record.date) << '\n';
		out.flush();
		const bool written = out.good();
		out.close();
		return written && !out.fail();
	}
	const std::vector<ScoreRecord>& GetRecords() const{ return records_; }
private:
	std::vector<ScoreRecord> records_;
};
