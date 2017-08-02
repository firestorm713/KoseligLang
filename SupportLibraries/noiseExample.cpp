int OctIndex = 0;
for (int y = 0; y < dimensions; y += stride)
{
	for (int xOffset = 0; xOffset < dimensions; xOffset += stride)
	{
		for (int x = 0; x < stride; x++)
		{
			float percentage = (float)x / (float)stride;
			Uint32 from = SDL_MapRGBA(fmt, octave1[OctIndex].r, octave1[OctIndex].g, octave1[OctIndex].b, octave1[OctIndex].a);
			Uint32 to = SDL_MapRGBA(fmt, octave1[OctIndex + 1].r, octave1[OctIndex + 1].g, octave1[OctIndex + 1].b, octave1[OctIndex + 1].a);
			Uint32 interpedColor = coserpColor(windowSurface, from, to, percentage);
			PutPixel32(windowSurface, x+xOffset, y, interpedColor);
		}
		OctIndex++;
	}
	if ((y + stride) == dimensions)
		y--;
	OctIndex++;
}
// Vertical Interpolation
for (int xOffset = 0; xOffset < dimensions; xOffset += stride)
{
	for (int yOffset = 0; yOffset < (dimensions-1); yOffset += stride)
	{
		if ((yOffset + stride) >= dimensions)
			yOffset--;
		for (int x = 0; x < stride; x++)
		{
			for (int y = 0; y < stride; y++)
			{
				//std::cout << "x is " << x << " y is " << y << " x+xOffset is " << (x + xOffset) << " y+yOffset is " << (y + yOffset) << "\n";
				float percentage = (float)y / (float)stride;
				Uint32 from = GetPixel(windowSurface, x + xOffset, yOffset);
				Uint32 to = GetPixel(windowSurface, x + xOffset, yOffset + stride);
				Uint32 interpedColor = coserpColor(windowSurface, from, to, percentage);
				PutPixel32(windowSurface, x + xOffset, y + yOffset, interpedColor);
			}
		}
	}
}