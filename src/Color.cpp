#include <cmath>
#include <iomanip>
#include <sstream>
#include "Color.h"

using namespace std;



const Color Color::BLACK(0);
const Color Color::WHITE(UInt8(255), UInt8(255), UInt8(255));


static inline UInt8 float_to_byte (float v) {
	return floor(255*v+0.5);
}


void Color::set (float r, float g, float b) {
	set(float_to_byte(r), float_to_byte(g), float_to_byte(b));
}


void Color::setHSB (float h, float s, float b) {
	vector<float> hsb(3), rgb(3);
	hsb[0] = h;
	hsb[1] = s;
	hsb[2] = b;
	HSB2RGB(hsb, rgb);
	set(rgb[0], rgb[1], rgb[2]);
}


void Color::setCMYK (float c, float m, float y, float k) {
	vector<float> cmyk(4), rgb(3);
	cmyk[0] = c;
	cmyk[1] = m;
	cmyk[2] = y;
	cmyk[3] = k;
	CMYK2RGB(cmyk, rgb);
	set(rgb[0], rgb[1], rgb[2]);
}


void Color::operator *= (double c) {
	UInt32 rgb=0;
	for (int i=0; i < 3; i++) {
		rgb |= UInt32(floor((_rgb & 0xff)*c+0.5)) << (8*i);
		_rgb >>= 8;
	}
	_rgb = rgb;
}


string Color::rgbString () const {
	ostringstream oss;
	oss << '#';
	for (int i=2; i >= 0; i--) {
		oss << setbase(16) << setfill('0') << setw(2) 
			 << (((_rgb >> (8*i)) & 0xff));
	}
	return oss.str();
}


/** Approximates a CMYK color by an RGB triple. The component values
 *  are expected to be normalized, i.e. 0 <= cmyk[i],rgb[j] <= 1.
 *  @param[in]  cmyk color in CMYK space
 *  @param[out] rgb  RGB approximation */
void Color::CMYK2RGB (const vector<float> &cmyk, vector<float> &rgb) {
	for (int i=0; i < 3; i++)
		rgb[i] = 1.0-min(1.0f, cmyk[i]+cmyk[3]);
}


/** Converts a color given in HSB coordinates to RGB. 
 *  @param[in]  hsb color in HSB space
 *  @param[out] rgb color in RGB space */
void Color::HSB2RGB (const vector<float> &hsb, vector<float> &rgb) {
	if (hsb[1] == 0) 
		rgb[0] = rgb[1] = rgb[2] = hsb[2];
	else {
		float h = hsb[0]-floor(hsb[0]);
		int i = 6*h;
		float f =  6*h-i;
		float p = hsb[2]*(1-hsb[1]);
		float q = hsb[2]*(1-hsb[1]*f);
		float t = hsb[2]*(1-hsb[1]*(1-f));
		switch (i) {
			case 0 : rgb[0]=hsb[2]; rgb[1]=t; rgb[2]=p; break;
			case 1 : rgb[0]=q; rgb[1]=hsb[2]; rgb[2]=p; break;
			case 2 : rgb[0]=p; rgb[1]=hsb[2]; rgb[2]=t; break;
			case 3 : rgb[0]=p; rgb[1]=q; rgb[2]=hsb[2]; break;
			case 4 : rgb[0]=t; rgb[1]=p; rgb[2]=hsb[2]; break;
			case 5 : rgb[0]=hsb[2]; rgb[1]=p; rgb[2]=q; break;
			default: ;  // prevent compiler warning
		}
	}
}