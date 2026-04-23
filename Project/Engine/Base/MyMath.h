#pragma once
#include <cmath>

// -------------------------------------------------
// Vector2
// -------------------------------------------------
struct Vector2{
	float x,y;

	Vector2& operator+=(const Vector2& other){
		x += other.x;
		y += other.y;
		return *this;
	}
};

inline bool operator<(const Vector2& a,const Vector2& b){
	if(a.x != b.x)
		return a.x < b.x;
	return a.y < b.y;
}

inline bool operator!=(const Vector2& a,const Vector2& b){
	return a.x != b.x || a.y != b.y;
}

// -------------------------------------------------
// Vector3
// -------------------------------------------------
struct Vector3{
	float x,y,z;
};

inline Vector3 Normalize(const Vector3& v){
	Vector3 result = {0, 0, 0};

	// ベクトルの長さを計算
	float length = std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

	// 0除算を防ぐ
	if(length != 0.0f){
		result.x = v.x / length;
		result.y = v.y / length;
		result.z = v.z / length;
	}

	return result;
}

inline bool operator<(const Vector3& a,const Vector3& b){
	if(a.x != b.x)
		return a.x < b.x;
	if(a.y != b.y)
		return a.y < b.y;
	return a.z < b.z;
}

inline bool operator!=(const Vector3& a,const Vector3& b){
	return a.x != b.x || a.y != b.y || a.z != b.z;
}

// Vector3 += Vector3
inline Vector3& operator+=(Vector3& lhv,const Vector3& rhv){
	lhv.x += rhv.x;
	lhv.y += rhv.y;
	lhv.z += rhv.z;
	return lhv;
}

// Vector3 + Vector3
inline Vector3 operator+(const Vector3& v1,const Vector3& v2){
	return {v1.x + v2.x, v1.y + v2.y, v1.z + v2.z};
}

// Vector3 * float
inline Vector3 operator*(const Vector3& v,float s){
	return {v.x * s, v.y * s, v.z * s};
}

// float * Vector3
inline Vector3 operator*(float s,const Vector3& v){
	return {v.x * s, v.y * s, v.z * s};
}

// -------------------------------------------------
// Vector4
// -------------------------------------------------
struct Vector4{
	float x,y,z,w;
};

inline bool operator<(const Vector4& a,const Vector4& b){
	if(a.x != b.x)
		return a.x < b.x;
	if(a.y != b.y)
		return a.y < b.y;
	if(a.z != b.z)
		return a.z < b.z;
	return a.w < b.w;
}

inline bool operator!=(const Vector4& a,const Vector4& b){
	return a.x != b.x || a.y != b.y || a.z != b.z || a.w != b.w;
}

// -------------------------------------------------
// Matrix & Transform
// -------------------------------------------------
struct Matrix4x4{
	float m[4][4];
};

struct Matrix3x3{
	float m[3][3];
};

struct Transform{
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

struct TransformationMatrix{
	Matrix4x4 WVP;
	Matrix4x4 World;
};

// -------------------------------------------------
// Matrix Functions
// -------------------------------------------------

inline Matrix4x4 MakeIdentity4x4(){
	Matrix4x4 result;
	for(int i = 0; i < 4; i++){
		for(int j = 0; j < 4; j++){
			if(i == j){
				result.m[i][j] = 1.0f;
			} else{
				result.m[i][j] = 0.0f;
			}
		}
	}
	return result;
}

inline Matrix4x4 MakeScaleMatrix(const Vector3& scale){
	Matrix4x4 matrix = {};
	matrix.m[0][0] = scale.x;
	matrix.m[1][1] = scale.y;
	matrix.m[2][2] = scale.z;
	matrix.m[3][3] = 1.0f;
	return matrix;
}

inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate){
	Matrix4x4 matrix = {};
	matrix.m[0][0] = 1.0f;
	matrix.m[1][1] = 1.0f;
	matrix.m[2][2] = 1.0f;
	matrix.m[3][3] = 1.0f;
	matrix.m[3][0] = translate.x;
	matrix.m[3][1] = translate.y;
	matrix.m[3][2] = translate.z;
	return matrix;
}

inline Matrix4x4 MakeRotateXMatrix(float radian){
	Matrix4x4 result{};
	result.m[0][0] = 1;
	result.m[3][3] = 1;
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = std::sin(radian);
	result.m[2][1] = -std::sin(radian);
	result.m[2][2] = std::cos(radian);
	return result;
}

inline Matrix4x4 MakeRotateYMatrix(float radian){
	Matrix4x4 result{};
	result.m[1][1] = 1.0f;
	result.m[3][3] = 1.0f;
	result.m[0][0] = std::cos(radian);
	result.m[0][2] = -std::sin(radian);
	result.m[2][0] = std::sin(radian);
	result.m[2][2] = std::cos(radian);
	return result;
}

inline Matrix4x4 MakeRotateZMatrix(float radian){
	Matrix4x4 result{};
	result.m[2][2] = 1;
	result.m[3][3] = 1;
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = std::sin(radian);
	result.m[1][0] = -std::sin(radian);
	result.m[1][1] = std::cos(radian);
	return result;
}

inline Matrix4x4 Multiply(const Matrix4x4& m1,const Matrix4x4& m2){
	Matrix4x4 result{};
	for(int row = 0; row < 4; ++row){
		for(int col = 0; col < 4; ++col){
			result.m[row][col] = 0.0f;
			for(int k = 0; k < 4; ++k){
				result.m[row][col] += m1.m[row][k] * m2.m[k][col];
			}
		}
	}
	return result;
}

inline Matrix4x4 MakeAffineMatrix(const Vector3& scale,const Vector3& rotate,const Vector3& translate){
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);
	Matrix4x4 rotateX = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateY = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZ = MakeRotateZMatrix(rotate.z);

	// 回転順: Z -> X -> Y
	Matrix4x4 rotateMatrix = Multiply(Multiply(rotateX,rotateY),rotateZ);
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	// Scale -> Rotate -> Translate
	Matrix4x4 affineMatrix = Multiply(Multiply(scaleMatrix,rotateMatrix),translateMatrix);

	return affineMatrix;
}

inline Matrix4x4 MakePerspectiveFovMatrix(float fovY,float aspectRatio,float nearClip,float farClip){
	float f = 1.0f / tanf(fovY * 0.5f);
	float range = farClip / (farClip - nearClip);

	Matrix4x4 result = {};
	result.m[0][0] = f / aspectRatio;
	result.m[1][1] = f;
	result.m[2][2] = range;
	result.m[2][3] = 1.0f;
	result.m[3][2] = -range * nearClip;
	result.m[3][3] = 0.0f;

	return result;
}

inline Matrix4x4 Inverse(const Matrix4x4& m){
	Matrix4x4 result{};

	float det =
		m.m[0][0] * (m.m[1][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
			m.m[1][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) +
			m.m[1][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) -
		m.m[0][1] * (m.m[1][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) -
			m.m[1][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
			m.m[1][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) +
		m.m[0][2] * (m.m[1][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) -
			m.m[1][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) +
			m.m[1][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) -
		m.m[0][3] * (m.m[1][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) -
			m.m[1][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) +
			m.m[1][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0]));

	if(det == 0){
		return result;
	}

	float invDet = 1.0f / det;

	result.m[0][0] = (m.m[1][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) - m.m[1][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) + m.m[1][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) * invDet;
	result.m[0][1] = (-m.m[0][1] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) + m.m[0][2] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) - m.m[0][3] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1])) * invDet;
	result.m[0][2] = (m.m[0][1] * (m.m[1][2] * m.m[3][3] - m.m[1][3] * m.m[3][2]) - m.m[0][2] * (m.m[1][1] * m.m[3][3] - m.m[1][3] * m.m[3][1]) + m.m[0][3] * (m.m[1][1] * m.m[3][2] - m.m[1][2] * m.m[3][1])) * invDet;
	result.m[0][3] = (-m.m[0][1] * (m.m[1][2] * m.m[2][3] - m.m[1][3] * m.m[2][2]) + m.m[0][2] * (m.m[1][1] * m.m[2][3] - m.m[1][3] * m.m[2][1]) - m.m[0][3] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1])) * invDet;

	result.m[1][0] = (-m.m[1][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) + m.m[1][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) - m.m[1][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) * invDet;
	result.m[1][1] = (m.m[0][0] * (m.m[2][2] * m.m[3][3] - m.m[2][3] * m.m[3][2]) - m.m[0][2] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) + m.m[0][3] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0])) * invDet;
	result.m[1][2] = -(m.m[0][0] * (m.m[1][2] * m.m[3][3] - m.m[1][3] * m.m[3][2]) - m.m[0][2] * (m.m[1][0] * m.m[3][3] - m.m[1][3] * m.m[3][0]) + m.m[0][3] * (m.m[1][0] * m.m[3][2] - m.m[1][2] * m.m[3][0])) * invDet;
	result.m[1][3] = (m.m[0][0] * (m.m[1][2] * m.m[2][3] - m.m[1][3] * m.m[2][2]) - m.m[0][2] * (m.m[1][0] * m.m[2][3] - m.m[1][3] * m.m[2][0]) + m.m[0][3] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0])) * invDet;

	result.m[2][0] = (m.m[1][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) - m.m[1][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) + m.m[1][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
	result.m[2][1] = (-m.m[0][0] * (m.m[2][1] * m.m[3][3] - m.m[2][3] * m.m[3][1]) + m.m[0][1] * (m.m[2][0] * m.m[3][3] - m.m[2][3] * m.m[3][0]) - m.m[0][3] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
	result.m[2][2] = (m.m[0][0] * (m.m[1][1] * m.m[3][3] - m.m[1][3] * m.m[3][1]) - m.m[0][1] * (m.m[1][0] * m.m[3][3] - m.m[1][3] * m.m[3][0]) + m.m[0][3] * (m.m[1][0] * m.m[3][1] - m.m[1][1] * m.m[3][0])) * invDet;
	result.m[2][3] = (-m.m[0][0] * (m.m[1][1] * m.m[2][3] - m.m[1][3] * m.m[2][1]) + m.m[0][1] * (m.m[1][0] * m.m[2][3] - m.m[1][3] * m.m[2][0]) - m.m[0][3] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])) * invDet;

	result.m[3][0] = (-m.m[1][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) + m.m[1][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) - m.m[1][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
	result.m[3][1] = (m.m[0][0] * (m.m[2][1] * m.m[3][2] - m.m[2][2] * m.m[3][1]) - m.m[0][1] * (m.m[2][0] * m.m[3][2] - m.m[2][2] * m.m[3][0]) + m.m[0][2] * (m.m[2][0] * m.m[3][1] - m.m[2][1] * m.m[3][0])) * invDet;
	result.m[3][2] = (-m.m[0][0] * (m.m[1][1] * m.m[3][2] - m.m[1][2] * m.m[3][1]) + m.m[0][1] * (m.m[1][0] * m.m[3][2] - m.m[1][2] * m.m[3][0]) - m.m[0][2] * (m.m[1][0] * m.m[3][1] - m.m[1][1] * m.m[3][0])) * invDet;
	result.m[3][3] = (m.m[0][0] * (m.m[1][1] * m.m[2][2] - m.m[1][2] * m.m[2][1]) - m.m[0][1] * (m.m[1][0] * m.m[2][2] - m.m[1][2] * m.m[2][0]) + m.m[0][2] * (m.m[1][0] * m.m[2][1] - m.m[1][1] * m.m[2][0])) * invDet;

	return result;
}

inline Matrix4x4 MakeOrthographicMatrix(float left,float top,float right,float bottom,float nearClip,float farClip){
	Matrix4x4 result = {};

	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = -nearClip / (farClip - nearClip);
	result.m[3][3] = 1.0f;

	return result;
}

inline Matrix4x4 Transpose(const Matrix4x4& m){
	Matrix4x4 result;
	for(int i = 0; i < 4; ++i){
		for(int j = 0; j < 4; ++j){
			result.m[i][j] = m.m[j][i];
		}
	}
	return result;
}