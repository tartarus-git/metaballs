float getDist(int2 coords, int2 metaball) {
	int width = coords.x - metaball.x;
	int height = coords.y - metaball.y;
	return (float)(width * width + height * height);
}

#define ONE_SIXTH (1.0f / 6.0f)

uint4 HSBToRGB(float hue, float saturation, float brightness) {
	uint4 result;
	//float temp = (1 - (hue - ONE_SIXTH) * 6) + (6 * hue - 1) * (1 - saturation)
	//1 - 6h + x + 6h - 6hs - x + xs
	// xs - 6hs + 1

	result.w = 0;
	if (hue > 5 * ONE_SIXTH) {
		result.z = (uint)round(brightness * 255);
		result.y = (uint)round((1 - saturation) * brightness * 255);
		result.x = (int)round((5 * saturation - 6 * hue * saturation + 1) * brightness * 255);
		return result;
	}
	if (hue > 4 * ONE_SIXTH) {
		result.z = (int)round((-4 * saturation + 6 * hue * saturation) * brightness * 255);
		result.y = (int)round((1 - saturation) * brightness * 255);
		result.x = (int)round(brightness * 255);
		return result;
	}
	if (hue > 3 * ONE_SIXTH) {
		result.z = (int)round((1 - saturation) * brightness * 255);
		result.y = (int)round((3 * saturation - 6 * hue * saturation + 1) * brightness * 255);
		result.x = (int)round(brightness * 255);
		return result;
	}
	if (hue > 2 * ONE_SIXTH) {
		result.z = (int)round((1 - saturation) * brightness * 255);
		result.y = (int)round(brightness * 255);
		result.x = (int)round((-2 * saturation + 6 * hue * saturation) * brightness * 255);
		return result;
	}
	if (hue > ONE_SIXTH) {
		result.z = (int)round((saturation - 6 * hue * saturation + 1) * brightness * 255);
		result.y = (int)round(brightness * 255);
		result.x = (int)round((1 - saturation) * brightness * 255);
		return result;
	}
	result.z = (int)round(brightness * 255);
	result.y = (int)round((6 * hue * saturation) * brightness * 255);
	result.x = (int)round((1 - saturation) * brightness * 255);
	return result;
}

__kernel void metaballRenderer(__global int2* positions, int positions_count, __write_only image2d_t outputFrame, 
							   int windowWidth, int windowHeight) {

	int x = get_global_id(0);
	if (x >= windowWidth) { return; }
	int2 coords = (int2)(x, get_global_id(1));

	float value = 0;
	for (int i = 0; i < positions_count; i++) {
		float temp = 20 / sqrt(getDist(coords, positions[i]));
		value += temp;
	}
	if (value > 0.0000f) {
		write_imageui(outputFrame, coords, HSBToRGB(clamp(value, 0.0f, 1.0f), 1, 1));
		return;
	}
	write_imageui(outputFrame, coords, (uint4)(0, 0, 0, 255));
}