#!/usr/bin/env python3
"""
Generate gradient background image for ESP32 car control display.

Creates a 320x240 PNG with vibrant race car inspired gradients.
Place the generated background.png on the SD card root.

Requirements:
    pip install pillow

Usage:
    python generate_background.py
    python generate_background.py --output /path/to/sd/background.png
"""

import argparse
import math
from PIL import Image, ImageDraw, ImageFilter


def lerp_color(c1, c2, t):
    """Linear interpolate between two RGB colors."""
    return tuple(int(c1[i] + (c2[i] - c1[i]) * t) for i in range(3))


def create_radial_gradient(draw, center, radius, inner_color, outer_color, width, height):
    """Draw a radial gradient (circular glow effect)."""
    for y in range(height):
        for x in range(width):
            dist = math.sqrt((x - center[0])**2 + (y - center[1])**2)
            t = min(dist / radius, 1.0)
            # Ease out for smoother falloff
            t = t * t
            color = lerp_color(inner_color, outer_color, t)
            draw.point((x, y), fill=color)


def create_race_background(width=320, height=240):
    """
    Create a vibrant e-bike/race car inspired background.
    
    Features:
    - Dark base with subtle gradient
    - Orange/cyan accent glows (common in racing themes)
    - Subtle vignette for depth
    """
    # Create base image with dark gradient
    img = Image.new('RGB', (width, height))
    draw = ImageDraw.Draw(img)
    
    # Dark base colors (Material Design 3 inspired)
    base_top = (0x14, 0x13, 0x18)      # Surface dim
    base_bottom = (0x0A, 0x0A, 0x0E)   # Even darker
    
    # Draw vertical gradient base
    for y in range(height):
        t = y / height
        color = lerp_color(base_top, base_bottom, t)
        draw.line([(0, y), (width, y)], fill=color)
    
    # Create glow layers
    # Orange glow (top-left, racing accent)
    glow1 = Image.new('RGB', (width, height), (0, 0, 0))
    glow1_draw = ImageDraw.Draw(glow1)
    orange_glow = (0xFF, 0x6B, 0x00)  # Vibrant orange
    for y in range(height):
        for x in range(width):
            # Distance from top-left corner
            dist = math.sqrt((x + 50)**2 + (y + 50)**2)
            intensity = max(0, 1 - dist / 300)
            intensity = intensity ** 2  # Ease out
            if intensity > 0:
                color = tuple(int(c * intensity * 0.15) for c in orange_glow)
                glow1_draw.point((x, y), fill=color)
    
    # Cyan glow (bottom-right, tech accent)
    glow2 = Image.new('RGB', (width, height), (0, 0, 0))
    glow2_draw = ImageDraw.Draw(glow2)
    cyan_glow = (0x00, 0xD4, 0xFF)  # Vibrant cyan
    for y in range(height):
        for x in range(width):
            # Distance from bottom-right
            dist = math.sqrt((x - width - 30)**2 + (y - height - 30)**2)
            intensity = max(0, 1 - dist / 350)
            intensity = intensity ** 2
            if intensity > 0:
                color = tuple(int(c * intensity * 0.12) for c in cyan_glow)
                glow2_draw.point((x, y), fill=color)
    
    # Red accent glow (center-right, subtle)
    glow3 = Image.new('RGB', (width, height), (0, 0, 0))
    glow3_draw = ImageDraw.Draw(glow3)
    red_glow = (0xFF, 0x20, 0x40)  # Racing red
    center_x, center_y = width - 80, height // 2
    for y in range(height):
        for x in range(width):
            dist = math.sqrt((x - center_x)**2 + (y - center_y)**2)
            intensity = max(0, 1 - dist / 200)
            intensity = intensity ** 3  # Tighter falloff
            if intensity > 0:
                color = tuple(int(c * intensity * 0.08) for c in red_glow)
                glow3_draw.point((x, y), fill=color)
    
    # Composite glows onto base using additive blending
    from PIL import ImageChops
    img = ImageChops.add(img, glow1)
    img = ImageChops.add(img, glow2)
    img = ImageChops.add(img, glow3)
    
    # Add subtle vignette
    vignette = Image.new('RGB', (width, height), (0, 0, 0))
    vignette_draw = ImageDraw.Draw(vignette)
    center_x, center_y = width // 2, height // 2
    max_dist = math.sqrt(center_x**2 + center_y**2)
    
    for y in range(height):
        for x in range(width):
            dist = math.sqrt((x - center_x)**2 + (y - center_y)**2)
            # Vignette intensity (darker at edges)
            t = dist / max_dist
            t = t ** 1.5  # Adjust curve
            darkness = int(t * 40)  # Max darkening
            vignette_draw.point((x, y), fill=(darkness, darkness, darkness))
    
    # Subtract vignette
    img = ImageChops.subtract(img, vignette)
    
    # Apply slight blur for smoothness
    img = img.filter(ImageFilter.GaussianBlur(radius=1))
    
    return img


def create_simple_gradient(width=320, height=240):
    """
    Create a simpler gradient background (faster to generate).
    Dark diagonal gradient with subtle color tints.
    """
    img = Image.new('RGB', (width, height))
    draw = ImageDraw.Draw(img)
    
    # Corner colors for 4-point gradient
    top_left = (0x1C, 0x1B, 0x1F)      # MD3 Surface
    top_right = (0x1A, 0x1D, 0x24)     # Slight blue tint
    bottom_left = (0x20, 0x18, 0x1A)   # Slight warm tint
    bottom_right = (0x14, 0x13, 0x18)  # MD3 Surface dim
    
    for y in range(height):
        for x in range(width):
            # Bilinear interpolation
            tx = x / width
            ty = y / height
            
            top = lerp_color(top_left, top_right, tx)
            bottom = lerp_color(bottom_left, bottom_right, tx)
            color = lerp_color(top, bottom, ty)
            
            draw.point((x, y), fill=color)
    
    return img


def main():
    parser = argparse.ArgumentParser(
        description='Generate gradient background for ESP32 display'
    )
    parser.add_argument(
        '--output', '-o',
        default='background.png',
        help='Output file path (default: background.png)'
    )
    parser.add_argument(
        '--width', '-W',
        type=int,
        default=320,
        help='Image width (default: 320)'
    )
    parser.add_argument(
        '--height', '-H',
        type=int,
        default=240,
        help='Image height (default: 240)'
    )
    parser.add_argument(
        '--simple', '-s',
        action='store_true',
        help='Use simpler/faster gradient (no glows)'
    )
    parser.add_argument(
        '--preview', '-p',
        action='store_true',
        help='Show preview window'
    )
    
    args = parser.parse_args()
    
    print(f"Generating {args.width}x{args.height} background...")
    
    if args.simple:
        img = create_simple_gradient(args.width, args.height)
    else:
        img = create_race_background(args.width, args.height)
    
    # Save as PNG
    img.save(args.output, 'PNG', optimize=True)
    print(f"Saved to: {args.output}")
    
    # Show file size
    import os
    size = os.path.getsize(args.output)
    print(f"File size: {size:,} bytes ({size/1024:.1f} KB)")
    
    if args.preview:
        img.show()


if __name__ == '__main__':
    main()
