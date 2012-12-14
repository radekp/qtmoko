(function(x, y, w, h)
{
    // Fullscreen toggle is in top left corner
    if(x < w / 8 && y < h / 8) { sym = -1; return true; }
    
    // Compute distances
    dLeft = x * x + (y - h / 2) * (y - h / 2);
    dRight = (w - x) * (w - x)  + (y - h / 2) * (y - h / 2);
    dUp = y * y + (x - w / 2) * (x - w / 2);
    dDown = (h - y) * (h - y) + (x - w / 2) * (x - w / 2);
    dEnter = (x - w / 2) * (x - w / 2) + (y - h / 2) * (y - h / 2);

    // Closest to left
    if(dLeft < dRight && dLeft < dUp && dLeft < dDown && dLeft < dEnter) {
        sym = 276;
        return true;
    }
    // Closest to right
    if(dRight < dUp && dRight < dDown && dRight < dEnter) {
        sym = 275;
        return true;
    }
    // Closest to up
    if(dUp < dDown && dUp < dEnter) {
        sym = 273;
        return true;
    }
    // Closest to down
    if(dDown < dEnter) {
        sym= 274;
        return true;
    }
    // Enter
    sym = 13;
    return true;
}
)
