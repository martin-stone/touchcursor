import sys
import os
import shutil
from glob import glob
import markdown

srcPath = "../docs"
templatePath = os.path.join(srcPath, "template.html")
helpPath = os.path.join(srcPath, "help.txt")
toCopy = ("key_layout.png", "screen_general.png", "screen_programs.png")

def makeDocs(destPath):
    
    if os.path.exists(destPath):
        shutil.rmtree(destPath)
        
    if not os.path.exists(destPath):
        os.mkdir(destPath)
        
    for file in toCopy:
        shutil.copy(os.path.join(srcPath, file), destPath)
    
    # markdown-process body and substitute in template
    body = open(helpPath).read()
    body = markdown.markdown(body)    
    template = open(templatePath).read()
    html = template.replace("$body", body)
    
    f = open(os.path.join(destPath, "help.html"), "wt")
    f.write(html)
    f.close()
    

if __name__ == "__main__":
    makeDocs(*sys.argv[1:])

