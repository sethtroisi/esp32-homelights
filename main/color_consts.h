#pragma once

// MatPlotLib Colors (license ~BSD)
// colors.LinearSegmentedColormap.from_list

/* Python to convert to uint16_t
_data = ( 
    (0.96862745098039216,  0.40784313725490196,  0.63137254901960782),
    (0.86666666666666667,  0.20392156862745098,  0.59215686274509804),
    (0.68235294117647061,  0.00392156862745098,  0.49411764705882355),
    (0.47843137254901963,  0.00392156862745098,  0.46666666666666667),
    (0.28627450980392155,  0.0                ,  0.41568627450980394)
)
name = "_RdPu_data"

UINT16_MAX = 2 ** 8 -  1
print (name, "= {")
for row in _data:
    print ("\t{", ", ".join([str(round(UINT16_MAX * v)) for v in row]), "},")

print ("};")

*/

const CRGB _RdPu_data[5] = {
	{ 247, 104, 161 },
	{ 221, 52, 151 },
	{ 174, 1, 126 },
	{ 122, 1, 119 },
	{ 73, 0, 106 },
};