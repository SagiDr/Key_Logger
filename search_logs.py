import sys #importing the sys module to handle command-line arguments and system-specific parameters
import re #importing the re module for regular expression operations

def clean_log_line(line):
    """Cleans up log commands to provide a more readable text output."""
    # Replace deletion markers with a visual symbol and remove modifier tags
    line = line.replace("[SPACE]", " ")
    line = line.replace("[ENTER]", "\n")
    # Strip special key tags that clutter the readability
    line = re.sub(r"\[L_SHIFT\]|\[R_SHIFT\]|\[L_CTRL\]|\[R_CTRL\]", "", line)
    return line

def search_in_log(file_path, keyword):
    try:
        with open(file_path, 'r', encoding='utf-8') as file:
            print(f"\n{'='*20}") # Print a separator line
            print(f"SEARCHING FOR: '{keyword}'")
            print(f"FILE: {file_path}")
            print(f"{'='*20}\n")
            
            found = False
            for line_number, line in enumerate(file, 1): //
                # Search for the keyword case-insensitively
                if re.search(keyword, line, re.IGNORECASE):
                    cleaned = clean_log_line(line.strip())
                    print(f"[{line_number:03d}] {cleaned}")
                    found = True
            
            if not found:
                print("No matches found.")
                
    except FileNotFoundError:
        print(f"Error: The file '{file_path}' was not found.")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    # Ensure correct usage arguments are provided
    if len(sys.argv) < 3:
        print("Usage: python3 search_logs.py <log_file> <keyword>")
    else:
        search_in_log(sys.argv[1], sys.argv[2])