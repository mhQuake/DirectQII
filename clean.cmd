attrib *.ncb -r -a -s -h /s /d
attrib *.sdf -r -a -s -h /s /d
attrib *.suo -r -a -s -h /s /d
attrib *.user -r -a -s -h /s /d

del *.ncb /s /q
del *.sdf /s /q
del *.suo /s /q
del *.user /s /q

rmdir ipch /s /q
rmdir Debug /s /q
rmdir Release /s /q

cd DirectQII

rmdir Debug /s /q
rmdir Release /s /q

cd ..
cd Renderer

rmdir Debug /s /q
rmdir Release /s /q

cd ..

echo done
pause
