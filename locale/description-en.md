# Gm Resource Modifier

**Author**: TheRedDaemon  
**Version**: 0.2.0  
**Repository**: [https://github.com/TheRedDaemon/ucp_gmResourceModifier](https://github.com/TheRedDaemon/ucp_gmResourceModifier)

This module exposes the possibility to to modify the loaded gm1 resources of the game. The gm files of Crusader are handled differently than the tgx files. While the latter are loaded from disc on demand, the former are loaded at the start. Replacing them requires more than intercepting and changing paths. For this reason, the control structures of the stored gm files were identified to replace them on demand.

One can replace either a whole file or individual pictures. They need to be of the same type, though, since SHC stores different formats in a gm1 file. As an additional feature, one can use this module to create a interface type single picture resource from a picture. The converter calls Windows functions, so the support depends on the OS (and what Wine supports).

This module currently provides no direct features and functions therefore as a low-level extension. For more infos, please check out the readme on the repository.