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

// Vector3 - Vector3
inline Vector3 operator-(const Vector3& v1,const Vector3& v2){
	return {v1.x - v2.x, v1.y - v2.y, v1.z - v2.z};
}

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
// -------------------------------------------------
// Quaternion & Interpolation Functions
// -------------------------------------------------
struct Quaternion{
	float x,y,z,w;
};

// Vector3の線形補間 (Lerp)
inline Vector3 Lerp(const Vector3& v1,const Vector3& v2,float t){
	return v1 + (v2 - v1) * t;
}

// Quaternionの球面線形補間 (Slerp)
inline Quaternion Slerp(const Quaternion& q1,const Quaternion& q2,float t){
	Quaternion result;

	// 2つのクォータニオンの内積を計算
	float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

	// 内積が負の場合、逆回りのルートを避けるために片方を反転する
	Quaternion targetQ2 = q2;
	if(dot < 0.0f){
		targetQ2.x = -q2.x;
		targetQ2.y = -q2.y;
		targetQ2.z = -q2.z;
		targetQ2.w = -q2.w;
		dot = -dot;
	}

	// 角度が非常に小さい場合は、誤差を防ぐために通常の線形補間(Lerp)に切り替える
	if(dot > 0.9995f){
		result.x = q1.x + t * (targetQ2.x - q1.x);
		result.y = q1.y + t * (targetQ2.y - q1.y);
		result.z = q1.z + t * (targetQ2.z - q1.z);
		result.w = q1.w + t * (targetQ2.w - q1.w);

		// 正規化
		float len = std::sqrtf(result.x * result.x + result.y * result.y + result.z * result.z + result.w * result.w);
		if(len != 0.0f){
			result.x /= len; result.y /= len; result.z /= len; result.w /= len;
		}
		return result;
	}

	// なす角を計算
	float theta_0 = std::acosf(dot);
	float theta = theta_0 * t;
	float sin_theta = std::sinf(theta);
	float sin_theta_0 = std::sinf(theta_0);

	float s0 = std::cosf(theta) - dot * sin_theta / sin_theta_0;
	float s1 = sin_theta / sin_theta_0;

	result.x = s0 * q1.x + s1 * targetQ2.x;
	result.y = s0 * q1.y + s1 * targetQ2.y;
	result.z = s0 * q1.z + s1 * targetQ2.z;
	result.w = s0 * q1.w + s1 * targetQ2.w;

	return result;
}