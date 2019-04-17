#ifndef _MATERIALS
#define _MATERIALS

#include "../Common/LightHelper.h"
#include "../Common/d3dUtil.h"

namespace Materials{
	static const Material Crystal_Diamond = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(2.4168, 0.17195, 0.0000) };
	static const Material Crystal_Germanium = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(5.6994, 0.52202, 1.6774) };
	static const Material Crystal_Ice = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.3098, 0.017986, 4.6784e-9) };
	static const Material Crystal_Quartz = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.4585, 0.034776, -1) };
	static const Material Crystal_Salt = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5442, 0.045753, -1) };
	static const Material Crystal_Silicon = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(3.9669, 0.35681, 0.0036217) };

	static const Material Metal_Aluminium = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.0972, 0.91320, 6.7943) };
	static const Material Metal_Copper = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(0.46090, 0.83204, 2.9736) };
	static const Material Metal_Gold = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(0.27049, 0.88412, 2.7789) };
	static const Material Metal_Iron = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(2.9304, 0.52050, 2.9996) };
	static const Material Metal_Mercury = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.7442, 0.78022, 4.9207) };
	static const Material Metal_Silver = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(0.15016, 0.95512, 3.4727) };

	static const Material Plastic_PMMA = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.4906, 0.038801, -1) };
	static const Material Plastic_PVP = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5274, 0.043550, 0.0020739) };
	static const Material Plastic_PS = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5916, 0.052113, -1) };
	static const Material Plastic_PC = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5848, 0.051182, -1) };
	static const Material Plastic_Cellulose = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.4906, 0.036224, -1) };
	static const Material Plastic_Optorez1330 = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5095, 0.041215, -1) };
	static const Material Plastic_NAS_21 = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5713, 0.049365, -1) };
	static const Material Plastic_ZeonexE48R = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5305, 0.043952, -1) };

	static const Material Liquid_Water = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 0.7), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.3325, 0.020320, 7.2792e-9) };
	static const Material Liquid_Acetone = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.3592, 0.23177, -1) };
	static const Material Liquid_Benzene = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.4957, 0.039456, -1) };
	static const Material Liquid_Mercury = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.7442, 0.78022, 4.9207) };

	static const Material Glass_BK7_optical_glass = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.6700, 0.062973, 1.8420e-8) };
	static const Material Glass_BAF10_optical_glass = {XMFLOAT4(1, 1, 1, 1),XMFLOAT4(1, 1, 1, 0.6),XMFLOAT4(1, 1, 1, 1),XMFLOAT3(1.5168, 0.042164, 9.7525e-9) };
	static const Material Glass_Soda_lime_glass_Clear = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5234, 0.043020, 3.8974e-7) };
	static const Material Glass_Soda_lime_glass_Bronze = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5234, 0.043020, 0.0000039119) };
	static const Material Glass_Soda_lime_glass_Gray = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5234, 0.043020, 0.0000063293) };
	static const Material Glass_Soda_lime_glass_Green = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1.5234, 0.043020, 0.0000019737) };
	//     ...

	static const Material Color_white_no_ambient = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };
	static const Material Color_white = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 1, 1, 1),XMFLOAT3(1, 1, 1)  };

	static const Material Color_lightgrey_no_ambient = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(0.5, 0.5, 0.5, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };
	static const Material Color_lightgrey = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0.5, 0.5, 0.5, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };

	static const Material Color_grey_no_ambient = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(0.25, 0.25, 0.25, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };
	static const Material Color_grey = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0.25, 0.25, 0.25, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };

	static const Material Color_blue_no_ambient = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(0, 0, 1, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };
	static const Material Color_blue = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 1, 1), XMFLOAT4(1, 1, 1, 1),XMFLOAT3(1, 1, 1) };

	static const Material Color_red_no_ambient = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(1, 0, 0, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };
	static const Material Color_red = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(1, 0, 0, 1), XMFLOAT4(1, 1, 1, 1),XMFLOAT3(1, 1, 1) };

	static const Material Color_lightblue_no_ambient = { XMFLOAT4(0, 0, 0, 1), XMFLOAT4(0, 0, 0.5f, 1), XMFLOAT4(1, 1, 1, 1), XMFLOAT3(1, 1, 1) };
	static const Material Color_lightblue = { XMFLOAT4(1, 1, 1, 1), XMFLOAT4(0, 0, 0.5f, 1), XMFLOAT4(1, 1, 1, 1),XMFLOAT3(1, 1, 1) };
	//     ...

	//material has 1.0f for each color component;
	//color has 1.0f for each physical component
	//FinalMaterial = Color*Material;

}

inline const Material operator+(const Material &left, const Material &right)
{
	Material sum;
	sum.Ambient = (XMFLOAT4)left.Ambient + (XMFLOAT4)right.Ambient;
	sum.Diffuse = (XMFLOAT4)left.Diffuse + (XMFLOAT4)right.Diffuse;
	sum.Specular = (XMFLOAT4)left.Specular + (XMFLOAT4)right.Specular;
	sum.Reflect = (XMFLOAT3)left.Reflect + (XMFLOAT3)right.Reflect;
	return sum;
};

inline const Material operator-(const Material &left, const Material &right)
{
	Material sum;
	sum.Ambient = (XMFLOAT4)left.Ambient - (XMFLOAT4)right.Ambient;
	sum.Diffuse = (XMFLOAT4)left.Diffuse - (XMFLOAT4)right.Diffuse;
	sum.Specular = (XMFLOAT4)left.Specular - (XMFLOAT4)right.Specular;
	sum.Reflect = (XMFLOAT3)left.Reflect - (XMFLOAT3)right.Reflect;
	return sum;
};

inline const Material operator*(const Material &left, const Material &right)
{
	Material sum;
	sum.Ambient = (XMFLOAT4)left.Ambient * (XMFLOAT4)right.Ambient;
	sum.Diffuse = (XMFLOAT4)left.Diffuse * (XMFLOAT4)right.Diffuse;
	sum.Specular = (XMFLOAT4)left.Specular * (XMFLOAT4)right.Specular;
	sum.Reflect = (XMFLOAT3)left.Reflect * (XMFLOAT3)right.Reflect;
	return sum;
};

#endif