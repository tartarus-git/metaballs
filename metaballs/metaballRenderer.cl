float getDist(uint2 coords, int2 metaball) {
    int width = coords.x - metaball.x;
    int height = coords.y = metaball.y;
    return sqrt(width * width + height * height);
}

__kernel void metaballRenderer(__global __read_only int2* positions, unsigned int positions_count, __write_only image2d_t outputFrame, 
                               unsigned int windowWidth, unsigned int windowHeight) {
    unsigned int x = get_global_id(0);
    if (x >= windowWidth) { return; }
    uint2 coords = (int2)(x, get_global_id(1));

    for (int i = 0; i < positions_count; i++) {
        float dist = getDist(coords, positions[i]);
        if (dist <= 20) {
            write_imagef(outputFrame, (1, 1, 1, 1));
            return;
        }
    }
}