draw_line(startx,starty,endx,endy,r,g,b) {
	t = 0.0;
	while(t < 1.0) {
		x = startx * (1.0 - t) + endx * t;
		y = starty * (1.0 - t) + endy * t;
		set_pixel(x,y,r,g,b);
		t += .01;
	}
}

draw_rect(x,y,size,r,g,b) {
	draw_line(x,y,x+size,y,r,g,b);
	draw_line(x+size,y,x+size,y+size,r,g,b);
	draw_line(x,y,x,y+size,r,g,b);
	draw_line(x,y+size,x+size,y+size,r,g,b);
}

draw_rect_filled(x,y,width,height,r,g,b) {
	//test_args(x,y,width,height,r,g,b);
	tmp_y = y;
	
	while(x < width) {
		y = tmp_y;
		while(y < height) {
			set_pixel(x,y,r,g,b);
			y++;
		}
		x++;
	}
}

draw_circle_filled(atx,aty,radius,r,g,b) {
	y = radius * -1;
	while(y <= radius) {
		x = radius * -1;
		while(x < radius) {
			if(x*x+y*y <= radius * radius) {
				set_pixel(atx + x, aty + y, r, g, b);
			}
			x++;
		}
		y++;
	}
}

clear_screen() {
	draw_rect_filled(0,0,vid_width,vid_height,vid_r,vid_g,vid_b);
}

rand_col() {
	col_r = rand() % 255;
	col_g = rand() % 255;
	col_b = rand() % 255;
}

shade_color(col) {
	tmp = col - 25;
	if(tmp < 0)
		tmp = 0;
	if(tmp > 255)
		tmp = 255;
	return tmp;
}

main() {
	vid_size=250;
	vid_width=vid_size;
	vid_height=vid_size;
	vid_r=255;
	vid_g=255;
	vid_b=255;
	
	clear_screen();
	
	MAX_CIRCLES=10;
	
	for(i = 0; i < MAX_CIRCLES; i++) {
		
		t = vid_width/MAX_CIRCLES;
		
		//col = 255 / MAX_CIRCLES * i;
		rand_col();
		draw_circle_filled(i * t, i * t, t, col_r,col_g,col_b);
		
		draw_circle_filled((i*t), (i*t), t/1.5, col_r - 30,col_g - 30,col_b - 30);
	}
}