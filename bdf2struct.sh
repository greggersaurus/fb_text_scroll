#!/bin/bash

# Usage: otf2bdf FreeSerif.ttf -p 32 | ./bdf2struct.sh
#
# Description: Script for converting Glyph Bitmap Distribution Format font
#  data to lookup table structs to be used by fb_scroll_text example program.
# Example usage above shows conversion of OpenType font into BDF and then
#  piping it into this script to genreate bdfchars.h, bdfchars_lut.h and
#  bitmap_array.h. These are included in main.cpp and used to dictate how a
#  requested character appears on the framebuffer display.

# Set to limit number of characters we process
max_char_hex_num=0x00FF

# Used to know which character we are processing
last_char_hex_num=0x0000
curr_char_hex_num=0x0000

# Output filenames
bitmap_arrays_filename=bitmap_array.h
bdfchars_filename=bdfchars.h
bdfchars_lut_filename=bdfchars_lut.h

# Clear headers. Starting each file with note that they were generated.
printf "/**\n" > $bitmap_arrays_filename
printf " * Note: Generated using bdf2struct.sh\n" >> $bitmap_arrays_filename
printf " */\n\n" >> $bitmap_arrays_filename

cp $bitmap_arrays_filename $bdfchars_filename
cp $bitmap_arrays_filename $bdfchars_lut_filename

# Start creation of LUT for accessing each character
printf "const tsBdfChar* const BDF_CHARS_LUT [] = \n" >> $bdfchars_lut_filename
printf "{ \n" >> $bdfchars_lut_filename

# Process each input line from odf2bdf
while IFS='' read -r line; 
do
	# Record character hex number
	if [[ $line == "STARTCHAR"* ]]
	then
		last_char_hex_num=$curr_char_hex_num
		curr_char_hex_num=0x${line:10}
		# Check if we have processed the desired number of characters
		#  and should stop or not
		if [[ "$curr_char_hex_num" -gt "$max_char_hex_num" ]]
		then
			echo "Done."
			break;
		fi

		# Create new struct for this character
		printf "const tsBdfChar msBdfChar$curr_char_hex_num = \n" >> $bdfchars_filename
		printf "{\n" >> $bdfchars_filename

		# Create link in LUT to struct,
		#  but first NULL out any spots if nessary
		# Special case for if no 0x0000 entry
		if [[ $last_char_hex_num == "0x0000" ]]
		then
			printf "\tNULL,\n" >> $bdfchars_lut_filename
		fi
		for (( i = 0; i < $curr_char_hex_num-$last_char_hex_num-1; i++ )); 
		do
			printf "\tNULL,\n" >> $bdfchars_lut_filename
		done
		printf "\t&msBdfChar$curr_char_hex_num,\n" >> $bdfchars_lut_filename
	fi

	# Add struct entry
	if [[ $line == "DWIDTH"* ]] 
	then
		string_array=($line)

		printf "\t.msDWidth = \n" >> $bdfchars_filename
		printf "\t{ \n" >> $bdfchars_filename

		printf "\t\t.x = ${string_array[1]},\n" >> $bdfchars_filename
		printf "\t\t.y = ${string_array[2]},\n" >> $bdfchars_filename

		printf "\t},\n" >> $bdfchars_filename
	fi	

	# Add struct entry
	if [[ $line == "BBX"* ]] 
	then
		string_array=($line)

		printf "\t.msBBX = \n" >> $bdfchars_filename
		printf "\t{ \n" >> $bdfchars_filename

		printf "\t\t.x = ${string_array[1]},\n" >> $bdfchars_filename
		printf "\t\t.y = ${string_array[2]},\n" >> $bdfchars_filename
		printf "\t\t.x_offset = ${string_array[3]},\n" >> $bdfchars_filename
		printf "\t\t.y_offset = ${string_array[4]},\n" >> $bdfchars_filename

		printf "\t},\n" >> $bdfchars_filename
	fi	

	# Create bitmap array and save to bitmap_arrays_filename
	if [[ $line == "BITMAP"* ]] 
	then
		# Add pointer to bitmap in struct
		printf "\t.maBitmap = maBitmap%s,\n" "$curr_char_hex_num" >> $bdfchars_filename

		# Start creation of bitmap array
		printf "const uint8_t maBitmap%s [] = \n" "$curr_char_hex_num" >> $bitmap_arrays_filename
		printf "{\n" >> $bitmap_arrays_filename

		# Convert each line of the BITMAP
		while IFS='' read -r line; 
		do
			if [[ $line == "ENDCHAR"* ]] 
			then
				# When we see ENDCHAR we know we've read the full BITMAP
				printf "};\n\n" >> $bitmap_arrays_filename
				# Close out struct entry as well
				printf "};\n\n" >> $bdfchars_filename
				break;	
			fi	

			printf "\t" >> $bitmap_arrays_filename
			for (( i=0; i < ${#line}; i+=2));
			do
				printf "0x%s," "${line:$i:2}" >> $bitmap_arrays_filename
			done
			printf "\n" >> $bitmap_arrays_filename
		done
	fi
done 

# Close out the look up table file
printf "};\n" >> $bdfchars_lut_filename

