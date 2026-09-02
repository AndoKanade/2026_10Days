#include "SpecialSelector.h"

#include <algorithm>

#include "Board.h"

bool SpecialSelector::Begin(const Board& board){
	// 積み上がっているマスを下段から探し、最初の選択位置にする
	for(int32_t y = board.GetHeight() - 1; y >= 0; --y){
		for(int32_t x = 0; x < board.GetWidth(); ++x){
			if(!board.GetCell(x,y).IsEmpty()){
				target_ = {x,y};
				isSelecting_ = true;
				return true;
			}
		}
	}

	return false;
}

void SpecialSelector::Move(int32_t deltaX,int32_t deltaY,const Board& board){
	if(!isSelecting_){
		return;
	}

	target_.x = std::clamp(target_.x + deltaX,0,board.GetWidth() - 1);
	target_.y = std::clamp(target_.y + deltaY,0,board.GetHeight() - 1);
}

void SpecialSelector::Cancel(){
	isSelecting_ = false;
}

bool SpecialSelector::CanConfirm(const Board& board) const{
	if(!isSelecting_ || !board.IsInside(target_.x,target_.y)){
		return false;
	}

	return !board.GetCell(target_.x,target_.y).IsEmpty();
}
