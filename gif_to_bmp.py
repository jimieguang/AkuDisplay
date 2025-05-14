from PIL import Image
import os
import glob

def convert_gif_to_bmp_sequence(gif_path, output_dir, max_width=162, max_height=132):
    """
    Convert a GIF file to a sequence of BMP images with proper scaling.
    
    Args:
        gif_path (str): Path to the input GIF file
        output_dir (str): Directory to save the BMP files
        max_width (int): Maximum width of output images
        max_height (int): Maximum height of output images
    """
    # Create output directory if it doesn't exist
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    # Open the GIF file
    with Image.open(gif_path) as gif:
        # Get the number of frames
        frame_count = gif.n_frames
        
        # Process each frame
        for frame_idx in range(frame_count):
            # Select the current frame
            gif.seek(frame_idx)
            
            # Convert to RGB if necessary
            frame = gif.convert('RGB')
            
            # Calculate scaling factor
            width, height = frame.size
            scale_width = max_width / width
            scale_height = max_height / height
            scale = min(scale_width, scale_height)
            
            # Calculate new dimensions
            new_width = int(width * scale)
            new_height = int(height * scale)
            
            # Resize the image
            resized = frame.resize((new_width, new_height), Image.Resampling.LANCZOS)
            
            # Save as BMP
            output_path = os.path.join(output_dir, f'frame_{frame_idx:04d}.bmp')
            resized.save(output_path, 'BMP')
            
            print(f'Processed frame {frame_idx + 1}/{frame_count}')

def process_all_gifs():
    # Get all GIF files in current directory
    gif_files = glob.glob('*.gif')
    
    if not gif_files:
        print("No GIF files found in current directory")
        return
    
    # Process each GIF file
    for i, gif_file in enumerate(gif_files, 1):
        output_dir = f'emotion{i}'
        print(f"\nProcessing {gif_file} -> {output_dir}")
        convert_gif_to_bmp_sequence(gif_file, output_dir)
        print(f"Completed processing {gif_file}")

if __name__ == '__main__':
    # 独立使用
    gif_path = r'C:\Users\11370\Desktop\Aku\开机.gif'  # Replace with your GIF file path
    output_dir = 'boot'
    convert_gif_to_bmp_sequence(gif_path, output_dir) 
    # 批量使用
    # process_all_gifs() 