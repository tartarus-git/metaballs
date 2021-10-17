#define RADIUS 20

float getDist(int2 coords, int2 metaball) {
	int width = coords.x - metaball.x;
	int height = coords.y - metaball.y;
	return sqrt((float)(width * width + height * height));
}

uint4 HSBToRGB(float hue, float saturation, float brightness) {
	uint4 result;

	result.w = 1;

	hue *= 6;
	unsigned int section = (unsigned int)hue;
	brightness *= 255;
	unsigned int brightestInt = (unsigned int)round(brightness);
	unsigned int oppositeColor = (unsigned int)round((1 - saturation) * brightness);

	switch (section) {
	case 0:
		result.z = brightestInt;
		result.y = (unsigned int)round((1 - saturation * (1 - hue)) * brightness);
		result.x = oppositeColor;
		return result;
	case 1:
		result.z = (unsigned int)round((1 - saturation * (hue - 1)) * brightness);
		result.y = brightestInt;
		result.x = oppositeColor;
		return result;
	case 2:
		result.z = oppositeColor;
		result.y = brightestInt;
		result.x = (unsigned int)round((1 - saturation * (3 - hue)) * brightness);
		return result;
	case 3:
		result.z = oppositeColor;
		result.y = (unsigned int)round((1 - saturation * (hue - 3)) * brightness);
		result.x = brightestInt;
		return result;
	case 4:
		result.z = (unsigned int)round((1 - saturation * (5 - hue)) * brightness);
		result.y = oppositeColor;
		result.x = brightestInt;
		return result;
	case 5:
		result.z = brightestInt;
		result.y = oppositeColor;
		result.x = (unsigned int)round((1 - saturation * (hue - 5)) * brightness);
		return result;
	case 6:
		result.z = brightestInt;
		result.y = oppositeColor;
		result.x = oppositeColor;
		return result;
	}
}

__kernel void metaballRenderer(__global int2* positions, unsigned int positions_count, __write_only image2d_t outputFrame, 
							   unsigned int windowWidth, unsigned int windowHeight) {

	int x = get_global_id(0);
	if (x >= windowWidth) { return; }
	int2 coords = (int2)(x, get_global_id(1));

	float value = 0;
	for (unsigned int i = 0; i < positions_count; i++) {
		float temp = RADIUS / getDist(coords, positions[i]);
		value += temp;
	}
	write_imageui(outputFrame, coords, HSBToRGB(fmin(value, 1), 1, 1));
}