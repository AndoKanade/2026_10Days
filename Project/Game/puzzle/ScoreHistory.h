#pragma once
#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

struct ScoreRecord{
	int64_t score = 0;
	int64_t cells = 0;
	std::string date;
};

// メモリ内だけで上位10件を保持する。ファイルの読み書きはしない。
class ScoreHistory{
public:
	void Add(const ScoreRecord& record){
		records_.push_back(record);
		// 同点では先に登録された記録を優先する。
		std::stable_sort(records_.begin(),records_.end(),[](const auto& a,const auto& b){ return a.score > b.score; });
		if(records_.size() > 10){ records_.resize(10); }
	}
	const std::vector<ScoreRecord>& GetRecords() const{ return records_; }
private:
	std::vector<ScoreRecord> records_;
};
