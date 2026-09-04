#include "SpecialSelector.h"

#include <algorithm>

#include "Board.h"

// 固定済みのマスが存在するときだけ選択を開始する
bool SpecialSelector::Begin(const Board& board){
	if(board.IsBusy()){
		return false;
	}
	// 積み上がっているマスを下段から探し、最初の選択位置にする
	for(int32_t y = board.GetHeight() - 1; y >= 0; --y){
		for(int32_t x = 0; x < board.GetWidth(); ++x){
			if(!board.GetCell(x,y).IsEmpty() && !board.GetCell(x,y).IsStrongest()){
				target_ = {x,y};
				isSelecting_ = true;
				return true;
			}
		}
	}

	return false;
}

//選択中のマスを上下左右に移動する。盤面の範囲外には出ない。
void SpecialSelector::Move(int32_t deltaX,int32_t deltaY,const Board& board){
	if(!isSelecting_){
		return;
	}

	target_.x = std::clamp(target_.x + deltaX,0,board.GetWidth() - 1);
	target_.y = std::clamp(target_.y + deltaY,0,board.GetHeight() - 1);
}

// 選択を終了する。ゲージの消費は行わない
void SpecialSelector::Cancel(){
	isSelecting_ = false;
}

// 選択中のマスが固定済みのマスかどうかを判定する
bool SpecialSelector::CanConfirm(const Board& board) const{
	if(!isSelecting_ || board.IsBusy() || !board.IsInside(target_.x,target_.y)){
		return false;
	}

	const Cell& cell = board.GetCell(target_.x,target_.y);
	return !cell.IsEmpty() && !cell.IsStrongest();
}
