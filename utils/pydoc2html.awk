# Qick hack to convert pydoc to something decent looking in html.
# Author: Thomas Moeller
# Usage: pydoc3.3 inventor | awk -f pydoc2html.awk > inventor.html
#
# Example for updating a man directory:
# for f in *.html; do pydoc3.3 ${f%%.html} | awk -f ../../PyInventor/utils/pydoc2html.awk > $f; done
#
# Copyright (C) the PyInventor contributors. All rights reserved.
# This file is part of PyInventor, distributed under the BSD 3-Clause
# License. For full terms see the included COPYING file.
#


BEGIN   {
          eatEmptyLines = 0;
          indendet = 0;
          dataDesc = 0;
          isBullet = 0;
          print "<!DOCTYPE html><html><head>";
          print "<meta name='description' content='PyInventor : 3D Graphics in Python with Open Inventor' />";
          print "<link rel='stylesheet' type='text/css' media='screen' href='../stylesheets/stylesheet.css'>";
          print "<title>PyInventor</title></head>";
          print "<body><div id='header_wrap' class='outer'><header class='inner'>";
          print "<a id='forkme_banner' href='https://github.com/TheHubbit/PyInventor'>View on GitHub</a>";
          print "<a href='http://thehubbit.github.io/PyInventor/'><h1 id='project_title'>PyInventor</h1></a><h2 id='project_tagline'>3D Graphics in Python with Open Inventor</h2>";
          print "</header></div>";

          print "<div id='main_content_wrap' class='outer'><section id='main_content' class='inner'>";
        }
        
END     {
          print "</div>";
          print "<div id='footer_wrap' class='outer'><footer class='inner'>";
          print "<p class='copyright'>PyInventor maintained by <a href='https://github.com/TheHubbit'>TheHubbit</a></p>";
          print "<p>Published with <a href='http://pages.github.com'>GitHub Pages</a></p></footer></div>";
          print "</body></html>";
        }

        { sub(/^[ \t\|]+/, ""); gsub(/[\<\>]/, ""); }
/^Help on/ { getline; next; }
/ = class / { print "<h1>" $1 "</h1><p><em>" $3 " " $4 "</em></p>"; module = substr($1, 0, index($1, ".") - 1); next; }
/^[ ]*FILE$/ { getline; next; }
/^[ ]*NAME$/ { getline; print "<h1>" $1 "</h1><p><em>module " $0 "</em></p>"; module = $1; next; }
/^[ ]*DESCRIPTION$/ { next; }
/^[ ]*FUNCTIONS$/ { print "<h2>Functions:</h2>"; next; }
/^[ ]*[A-Z]+$/ { print "<h2>" $0 "</h2>"; next; }
/PACKAGE CONTENTS/ {
            print "<h2>Package contents:</h2><ul>";
            while (!match($0, /^[ \|\t\n]*$/))
            { 
              getline;
              if ((length($0) > 2) && !match($0, "__main__"))
              {
                print "<li><a href='" module "." $1 ".html'>" $0 "</a>";
              }
            }
            print "</ul>"; 
            isBullet = 0; next; 
          }
/Method resolution order:/ {
            print "<p><em>" $0 "</em></p><ul>"; 
            while (!match($0, /^[ \|\t]*$/)) 
            { 
              getline;
              if (match($2, /PySide\./)) { link = $2; gsub(/\./, "/", link); print "<li><a href='http://srinikom.github.io/pyside-docs/" link "'>" $2 "</a>"; } else
              if (match($2, /FieldContainer/)) print "<li><a href='" module "." $2 ".html'>" $2 "</a>"; else
              if (length($2) > 0) print "<li>" $2;
            } 
            print "</ul>"; 
            isBullet = 0; next; 
          }
/Methods inherited from PySide/ { exit; }
/^[ ]*[A-Z][a-z]+ [A-Za-z \.]+:$/ { header = "<h2> " $0 "</h2>"; eatEmptyLines = 1; if (match($0, /[ ]*Data /)) dataDesc = 1; next; }
/__/    { getline; next; }
/^[ \t]*[a-zA-Z0-9_]+\([A-Za-z0-9_\., ]+\)$/ {
          sub(/^[ \t]*/, "");
          if (indendet)
          {
            indendet = 0;
            print "</ul>";
          }
          if (length(header) > 0)
          {
            print header;
            header = "";
          }
          print "<h3>" $0 "</h3>"; next;
        }
/Args:|Returns:|Note:/ {
          if (indendet)
          {
            indendet = 0;
            print "</ul>";
          }
          print "<p><em>" $0 "</em></p>"; print "<ul>"; indendet = 1; next;
        }
/^[ ]*\- [a-zA-Z0-9_, ]+: / {
          if (!isBullet) print "<ul>";
          target = substr($2, 0, length($2) -1);
          if (target == "inventor") print "<li><a href='" target ".html'>" $2 "</a>";
          else if (match(target, /^[A-Z]/)) print "<li><a href='" module "." target ".html'>" $2 "</a>";
          else print "<li>" $2;

          sub(/^[ ]*\- [a-zA-Z0-9_, ]+: /, ""); 
          print; 
          isBullet = 1; next; 
        }
/^[ -]*[a-zA-Z0-9_, ]+: / { sub(/^[ -]+/, ""); print "<li>" $0; isBullet = 1; next; }
/-----/ { print ""; dataDesc = 0; next; }
        {
          sub(/^[ \t]*/, "");
          if (dataDesc)
          {
            if (indendet)
            {
              indendet = 0;
              print "</ul>";
            }
            if ((length(header) > 0) && (length($0) > 0))
            {
              print header;
              header = "";
            }
            if (match($0, /^[ \t]*[a-zA-Z0-9_]+$/))
            {
              print "<h3>" $0 "</h3>";
            }
            else if (length($0) > 0)
            {
              if (index($0, " = ") > 0)
              {
                print $0 "<br />";
              }
              else print $0;
            }
            else print "<br />";
          }
          else if (!eatEmptyLines || length($0) > 0)
          {
            print;
          }

          if (!isBullet && !eatEmptyLines && length($0) == 0)
          {
            print "<p />";
          }

          if (length($0) > 0) eatEmptyLines = 0; else if (isBullet)
          {
            print "</ul>";
            isBullet = 0;
          }
        }

