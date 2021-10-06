float getDist(int2 coords, int2 metaball) {
    int width = coords.x - metaball.x;
    int height = coords.y - metaball.y;
    //return sqrt((float)(width * width + height * height));
    return (float)(width * width + height * height);
}

__kernel void metaballRenderer(__global int2* positions, int positions_count, __write_only image2d_t outputFrame, 
                               int windowWidth, int windowHeight) {
    int x = get_global_id(0);
    if (x >= windowWidth) { return; }
    int2 coords = (int2)(x, get_global_id(1));

    float dist = 0;
    for (int i = 0; i < positions_count; i++) {
        dist += 1 / sqrt(getDist(coords, positions[i]));
    }
    if (dist >= 0.025) {
        write_imagef(outputFrame, coords, (float4)(dist, 0, 0, 1));
        return;
    }
}