export inline void FillValues(uniform float firstArray[], uniform float secondArray[], uniform const float firstValue, uniform const float secondValue, uniform const size_t size)
{
	foreach(i = 0 ... size)
	{
		firstArray[i] = firstValue;
		secondArray[i] = secondValue;
	}
}

export inline void SimpleCopy(uniform const float inArray[], uniform float outArray[], uniform const size_t size)
{
	foreach(i = 0 ... size)
	{
		outArray[i] = inArray[i];
	}
}

export inline void ColorShift(uniform uint32 inArray[], uniform const size_t width, uniform const size_t height, uniform const float redScale, uniform const float greenScale, uniform const float blueScale)
{
	foreach_tiled(x = 0 ... width, y = 0 ... height)
	{
		const uint32 pixelIndex = (y * width) + x;
		
		// A = 0x000000FF
		// G = 0x0000FF00
		// R = 0x00FF0000
		// B = 0xFF000000
		uint32 originalPixel = inArray[pixelIndex];
		float rValue = (originalPixel & 0x00FF0000) >> 16;
		float gValue = (originalPixel & 0x0000FF00) >> 8;
		float bValue = (originalPixel & 0xFF000000) >> 24;
		const uint32 redShift = ((uint32)(rValue * redScale) << 16) & 0x00FF0000;
		const uint32 greenShift = ((uint32)(gValue * greenScale) << 8) & 0x0000FF00;
		const uint32 blueShift = ((uint32)(bValue * blueScale) << 24) & 0xFF000000;
		originalPixel &= 0x000000FF;
		originalPixel |= redShift;
		originalPixel |= greenShift;
		originalPixel |= blueShift;
		inArray[pixelIndex] = originalPixel;
	}
}
