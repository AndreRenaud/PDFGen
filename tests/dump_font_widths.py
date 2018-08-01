#!/usr/bin/env python

from fpdf import FPDF
import sys

pdf = FPDF()

fonts = [("Helvetica", ""),
		 ("Helvetica", "B"),
		 #("Helvetica", "BO"),
		 #("Helvetica", "O"),
		 ("Symbol", ""),
		 ("Times", ""),
		 ("Times", "B"),
		 ("Times", "BI"),
		 ("Times", "I"),
		 ("ZapfDingbats", ""),
		 ("Courier", "")]

for f in fonts:
	pdf.set_font(f[0], f[1], 14)
	font = f[0].lower()
	style = f[1].lower()
	if style == "b":
		style = "_bold"
	if style == "bi":
		style = "_bold_italic"
	if style == "bo":
		style = "_bold_oblique"
	if style == "i":
		style = "_italic"
	if style == "o":
		style = "_oblique"

	sys.stdout.write("static const uint16_t %s%s_widths[256] = {\n" % (font, style))
	for i in range(0, 256):
		if i % 14 == 0:
			sys.stdout.write("    ")
		width = int(pdf.get_string_width(chr(i)) * 2.83465 * 72)
		sys.stdout.write("%d, " % width)
		if i % 14 == 13:
			sys.stdout.write("\n")
	sys.stdout.write("\n};\n\n")
