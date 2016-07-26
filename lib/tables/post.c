#include "post.h"

// clang-format off
const char *standardMacNames[258] = {".notdef", ".null", "nonmarkingreturn", "space", "exclam", "quotedbl", "numbersign", "dollar", "percent", "ampersand", "quotesingle", "parenleft", "parenright", "asterisk", "plus", "comma", "hyphen", "period", "slash", "zero", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "colon", "semicolon", "less", "equal", "greater", "question", "at", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "bracketleft", "backslash", "bracketright", "asciicircum", "underscore", "grave", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "braceleft", "bar", "braceright", "asciitilde", "Adieresis", "Aring", "Ccedilla", "Eacute", "Ntilde", "Odieresis", "Udieresis", "aacute", "agrave", "acircumflex", "adieresis", "atilde", "aring", "ccedilla", "eacute", "egrave", "ecircumflex", "edieresis", "iacute", "igrave", "icircumflex", "idieresis", "ntilde", "oacute", "ograve", "ocircumflex", "odieresis", "otilde", "uacute", "ugrave", "ucircumflex", "udieresis", "dagger", "degree", "cent", "sterling", "section", "bullet", "paragraph", "germandbls", "registered", "copyright", "trademark", "acute", "dieresis", "notequal", "AE", "Oslash", "infinity", "plusminus", "lessequal", "greaterequal", "yen", "mu", "partialdiff", "summation", "product", "pi", "integral", "ordfeminine", "ordmasculine", "Omega", "ae", "oslash", "questiondown", "exclamdown", "logicalnot", "radical", "florin", "approxequal", "Delta", "guillemotleft", "guillemotright", "ellipsis", "nonbreakingspace", "Agrave", "Atilde", "Otilde", "OE", "oe", "endash", "emdash", "quotedblleft", "quotedblright", "quoteleft", "quoteright", "divide", "lozenge", "ydieresis", "Ydieresis", "fraction", "currency", "guilsinglleft", "guilsinglright", "fi", "fl", "daggerdbl", "periodcentered", "quotesinglbase", "quotedblbase", "perthousand", "Acircumflex", "Ecircumflex", "Aacute", "Edieresis", "Egrave", "Iacute", "Icircumflex", "Idieresis", "Igrave", "Oacute", "Ocircumflex", "apple", "Ograve", "Uacute", "Ucircumflex", "Ugrave", "dotlessi", "circumflex", "tilde", "macron", "breve", "dotaccent", "ring", "cedilla", "hungarumlaut", "ogonek", "caron", "Lslash", "lslash", "Scaron", "scaron", "Zcaron", "zcaron", "brokenbar", "Eth", "eth", "Yacute", "yacute", "Thorn", "thorn", "minus", "multiply", "onesuperior", "twosuperior", "threesuperior", "onehalf", "onequarter", "threequarters", "franc", "Gbreve", "gbreve", "Idotaccent", "Scedilla", "scedilla", "Cacute", "cacute", "Ccaron", "ccaron", "dcroat"};
// clang-format on
table_post *caryll_new_post() {
	table_post *post = (table_post *)calloc(1, sizeof(table_post));
	post->version = 0x30000;
	return post;
}
table_post *caryll_read_post(caryll_packet packet) {
	FOR_TABLE('post', table) {
		font_file_pointer data = table.data;

		table_post *post = (table_post *)malloc(sizeof(table_post) * 1);
		post->version = read_32s(data);
		post->italicAngle = read_32u(data + 4);
		post->underlinePosition = read_16u(data + 8);
		post->underlineThickness = read_16u(data + 10);
		post->isFixedPitch = read_32u(data + 12);
		post->minMemType42 = read_32u(data + 16);
		post->maxMemType42 = read_32u(data + 20);
		post->minMemType1 = read_32u(data + 24);
		post->maxMemType1 = read_32u(data + 28);
		post->post_name_map = NULL;
		// Foamt 2 additional glyph names
		if (post->version == 0x20000) {
			glyph_order_hash *map;
			NEW(map);
			*map = NULL;

			sds pendingNames[0x10000];
			memset(pendingNames, 0, sizeof(pendingNames));
			uint16_t numberGlyphs = read_16u(data + 32);
			uint32_t offset = 34 + 2 * numberGlyphs;
			uint16_t pendingNameIndex = 0;
			while (pendingNameIndex <= 0xFFFF && offset < table.length) {
				uint8_t len = data[offset];
				sds s;
				if (len > 0) {
					s = sdsnewlen(data + offset + 1, len);
				} else {
					s = sdsempty();
				}
				offset += len + 1;
				pendingNames[pendingNameIndex] = s;
				pendingNameIndex += 1;
			}
			for (uint16_t j = 0; j < numberGlyphs; j++) {
				uint16_t nameMap = read_16u(data + 34 + 2 * j);
				if (nameMap >= 258) { // Custom glyph name
					try_name_glyph(map, j, sdsdup(pendingNames[nameMap - 258]));
				} else { // Standard Macintosh glyph name
					try_name_glyph(map, j, sdsnew(standardMacNames[nameMap]));
				}
			}
			for (uint32_t j = 0; j < pendingNameIndex; j++) { sdsfree(pendingNames[j]); }
			post->post_name_map = map;
		}
		return post;
	}
	return NULL;
}

void caryll_delete_post(table_post *table) {
	if (table->post_name_map != NULL) { delete_glyph_order_map(table->post_name_map); }
	free(table);
}

void caryll_post_to_json(table_post *table, json_value *root, caryll_options *options) {
	if (!table) return;
	if (options->verbose) fprintf(stderr, "Dumping post.\n");

	json_value *post = json_object_new(10);
	json_object_push(post, "version", json_double_new(caryll_from_fixed(table->version)));
	json_object_push(post, "italicAngle", json_integer_new(caryll_from_fixed(table->italicAngle)));
	json_object_push(post, "underlinePosition", json_integer_new(table->underlinePosition));
	json_object_push(post, "underlineThickness", json_integer_new(table->underlineThickness));
	json_object_push(post, "isFixedPitch", json_boolean_new(table->isFixedPitch));
	json_object_push(post, "minMemType42", json_integer_new(table->minMemType42));
	json_object_push(post, "maxMemType42", json_integer_new(table->maxMemType42));
	json_object_push(post, "minMemType1", json_integer_new(table->minMemType1));
	json_object_push(post, "maxMemType1", json_integer_new(table->maxMemType1));
	json_object_push(root, "post", post);
}
table_post *caryll_post_from_json(json_value *root, caryll_options *options) {
	table_post *post = caryll_new_post();
	json_value *table = NULL;
	if ((table = json_obj_get_type(root, "post", json_object))) {
		if (options->verbose) fprintf(stderr, "Parsing post.\n");
		if (options->short_post) {
			post->version = 0x30000;
		} else {
			post->version = caryll_to_fixed(json_obj_getnum(table, "version"));
		}
		post->italicAngle = caryll_to_fixed(json_obj_getnum(table, "italicAngle"));
		post->underlinePosition = json_obj_getnum(table, "underlinePosition");
		post->underlineThickness = json_obj_getnum(table, "underlineThickness");
		post->isFixedPitch = json_obj_getbool(table, "isFixedPitch");
		post->minMemType42 = json_obj_getnum(table, "minMemType42");
		post->maxMemType42 = json_obj_getnum(table, "maxMemType42");
		post->minMemType1 = json_obj_getnum(table, "minMemType1");
		post->maxMemType1 = json_obj_getnum(table, "maxMemType1");
	}
	return post;
}
caryll_buffer *caryll_write_post(table_post *post, glyph_order_hash *glyphorder,
                                 caryll_options *options) {
	caryll_buffer *buf = bufnew();
	if (!post) return buf;
	bufwrite32b(buf, post->version);
	bufwrite32b(buf, post->italicAngle);
	bufwrite16b(buf, post->underlinePosition);
	bufwrite16b(buf, post->underlineThickness);
	bufwrite32b(buf, post->isFixedPitch);
	bufwrite32b(buf, post->minMemType42);
	bufwrite32b(buf, post->maxMemType42);
	bufwrite32b(buf, post->minMemType1);
	bufwrite32b(buf, post->maxMemType1);
	if (post->version == 0x20000) {
		bufwrite16b(buf, HASH_COUNT(*glyphorder));
		// Since the glyphorder is already sorted using the "real" glyph order
		// we can simply write down the glyph names.
		glyph_order_entry *s;
		foreach_hash(s, *glyphorder) { bufwrite16b(buf, 258 + s->gid); }
		foreach_hash(s, *glyphorder) {
			bufwrite8(buf, sdslen(s->name));
			bufwrite_sds(buf, s->name);
		}
	}
	return buf;
}
