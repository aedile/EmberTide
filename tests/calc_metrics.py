import json
from PIL import Image

def calc_font_metrics(path):
    img = Image.open(path).convert('RGBA')
    metrics = []
    for i in range(95):
        r, c = i // 19, i % 19
        sx, sy = c * 30, r * 30
        crop = img.crop((sx, sy, sx+30, sy+30))
        # Find all opaque pixels (x, y)
        pxs = [(x, y) for x in range(30) for y in range(30) if crop.getpixel((x, y))[3] > 10]
        if not pxs:
            metrics.append({'w': 8 if '12' in path else (12 if '18' in path else 16), 'h': 30, 'dx': 0, 'dy': 0})
        else:
            xs = [p[0] for p in pxs]
            ys = [p[1] for p in pxs]
            min_x, max_x = min(xs), max(xs)
            min_y, max_y = min(ys), max(ys)
            
            # Give +1 right padding to all glyphs for kerning
            w = max_x - min_x + 2 
            metrics.append({
                'w': w, 
                'h': max_y - min_y + 1, 
                'dx': min_x, 
                'dy': min_y
            })
    return metrics

metrics = {
    'FONT_REGS_12': calc_font_metrics('assets/Fonts/FONT_REGS_12.png'),
    'FONT_REGS_18': calc_font_metrics('assets/Fonts/FONT_REGS_18.png'),
    'FONT_REGS_24': calc_font_metrics('assets/Fonts/FONT_REGS_24.png'),
    'FONT_SCRIPT_24': calc_font_metrics('assets/Fonts/FONT_SCRIPT_24.png'),
    'FONT_SCRIPT_36': calc_font_metrics('assets/Fonts/FONT_SCRIPT_36.png'),
}

with open('tests/font_metrics.json', 'w') as f:
    json.dump(metrics, f, indent=2)

print("Saved tests/font_metrics.json")
