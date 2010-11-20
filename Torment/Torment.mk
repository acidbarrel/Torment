##
## Auto Generated makefile by CodeLite IDE
## any manual changes will be erased      
##
## Debug
ProjectName            :=Torment
ConfigurationName      :=Debug
IntermediateDirectory  :=Debug
OutDir                 := $(IntermediateDirectory)
WorkspacePath          := "/home/david/Torment/Torment"
ProjectPath            := "/home/david/Torment/Torment"
CurrentFileName        :=
CurrentFilePath        :=
CurrentFileFullPath    :=
User                   :=David
Date                   :=10-11-18
CodeLitePath           :="/home/david/.codelite"
LinkerName             :=g++
ArchiveTool            :=ar rcus
SharedObjectLinkerName :=g++ -shared -fPIC
ObjectSuffix           :=.o
DependSuffix           :=.o.d
PreprocessSuffix       :=.o.i
DebugSwitch            :=-gstab
IncludeSwitch          :=-I
LibrarySwitch          :=-l
OutputSwitch           :=-o 
LibraryPathSwitch      :=-L
PreprocessorSwitch     :=-D
SourceSwitch           :=-c 
CompilerName           :=g++
C_CompilerName         :=gcc
OutputFile             :=$(IntermediateDirectory)/$(ProjectName).exe
Preprocessors          :=
ObjectSwitch           :=-o 
ArchiveOutputSwitch    := 
PreprocessOnlySwitch   :=-E 
MakeDirCommand         :=mkdir -p
CmpOptions             := /Zi $(Preprocessors)
LinkOptions            :=  /DEBUG
IncludePath            :=  "$(IncludeSwitch)." 
RcIncludePath          :=
Libs                   :=
LibPath                := "$(LibraryPathSwitch)." 


##
## User defined environment variables
##
CodeLiteDir:=/usr/share/codelite
Objects=

##
## Main Build Targets 
##
all: $(OutputFile)

$(OutputFile): makeDirStep $(Objects)
	@$(MakeDirCommand) $(@D)
	$(LinkerName) $(OutputSwitch)$(OutputFile) $(Objects) $(LibPath) $(Libs) $(LinkOptions)

makeDirStep:
	@test -d Debug || $(MakeDirCommand) Debug

PreBuild:


##
## Objects
##

-include $(IntermediateDirectory)/*$(DependSuffix)
##
## Clean
##
clean:
	$(RM) $(OutputFile)


