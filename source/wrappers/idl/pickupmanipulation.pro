;------------------------------------------------------
; FUNCTION: pickupmanipulation
; AUTHOR: L. Kogan
; DATE: March 2016
;
; Function to take data structure from getgeomdata, containing pickup
; coil data, and apply manipulations on the orientation and geometry information:
; - Project the length onto the poloidal plane
; - Calculate the angle of the coils in the poloidal plane
; - Calculate the fraction in which the coils measure in the R, Z &
;   Phi directions
;
; Arguments:
;   data: data structure from getgeomdata
;
; Keywords:
;   /poloidal : set to do poloidal manipulations
;   /plot : set to plot locations of pickup coils
;------------------------------------------------------

;------------------------------------------------------
; Convert unit vector to normalised components
;------------------------------------------------------
pro vectorTobRbZbPhi, orien_r, orien_z, orien_phi, bR=bR, bZ=bZ, bPhi=bPhi
  norm = abs(orien_r)+abs(orien_z)+abs(orien_phi)

  bPhi=orien_phi/norm
  bR=orien_r/norm
  bZ=orien_z/norm
end

;------------------------------------------------------
; From the length and toroidal angle, calculate
; the length of the coil projected onto the poloidal plane
;------------------------------------------------------
function lengthpolproj, length, orien_phi
  angle_to_poloidal_plane = asin(orien_phi)
  fraction_poloidal_plane = cos(angle_to_poloidal_plane)

  return, length*fraction_poloidal_plane
end

;------------------------------------------------------
; Get the poloidal angle of the coil
;------------------------------------------------------
function getPolAngle, R, Z

  theta = fltarr(n_elements(R))

  ind_nonzero = where(((R ne 0) and (Z ne 0)), count_nonzero)
  ind_rzzero = where((R eq 0) and (Z eq 0), count_rzzero)
  ind_rzero = where(R eq 0, count_rzero)
  ind_zzero = where(Z eq 0, count_zzero)

  if count_nonzero gt 0 then begin
     theta[ind_nonzero] = acos(R[ind_nonzero]/sqrt(R[ind_nonzero]*R[ind_nonzero]+Z[ind_nonzero]*Z[ind_nonzero]))*180.0/!PI

     if Z[ind_nonzero] gt 0.0 then theta[ind_nonzero] = 360.0 - theta[ind_nonzero]

  endif else if count_rzero gt 0 then begin
     theta[ind_rzero] = 270
  endif

  if count_rzzero gt 0 then theta[ind_rzzero] = 0

  return, theta
end

;------------------------------------------------------
; Find:
; - Project the length onto the poloidal plane
; - Calculate the angle of the coils in the poloidal plane
; - Calculate the fraction in which the coils measure in the R, Z &
;   Phi directions
; Add the poloidal length to the geometry structure, and
; the poloidal angle and fractions to the orientation structure.
;------------------------------------------------------
pro pickup_poloidal, geometry, orientation

  ; Length projected to poloidal plane
  length_poloidal = lengthpolproj(geometry.length, $
                                  orientation.unit_vector.phi)  

  length = geometry.length
  nturnsTotal = geometry.nturnsTotal
  areaAve = geometry.areaAve

  geometry = replicate({ length: 0.0,    $
                         nturnsTotal: 0, $
                         areaAve: 0.0,   $
                         length_poloidal: 0.0 }, n_elements(geometry))

  geometry.length = length
  geometry.nturnsTotal = nturnsTotal
  geometry.areaAve = areaAve
  geometry.length_poloidal = length_poloidal

  ; Angle in poloidal plane
  poloidal_angle = getPolAngle(orientation.unit_vector.r, orientation.unit_vector.z) 
  
  ; Fraction measured in bR, bZ, bPhi directions
  vectorTobRbZbPhi, orientation.unit_vector.r, orientation.unit_vector.z, orientation.unit_vector.phi, bR=bR, bZ=bZ, bPhi=bPhi

  measurement_direction = orientation.measurement_direction

  orientation = replicate({ measurement_direction: '', $
                            bRFraction: 0.0,   $
                            bZFraction: 0.0,   $
                            bPhiFraction: 0.0, $
                            poloidal_angle: 0.0 }, n_elements(orientation))
  orientation.measurement_direction = measurement_direction
  orientation.bRFraction = bR
  orientation.bZFraction = bZ
  orientation.bPhiFraction = bPhi
  orientation.poloidal_angle = poloidal_angle 
end

;------------------------------------------------------
; Recursively loop over the data structure, looking for 
; the level where the orientation and geometry data is
;------------------------------------------------------
function pickuploop, data

  forward_function pickuploop

  tag_names_data = tag_names(data)

  ind_orien = where(strmatch(tag_names_data, 'ORIENTATION') eq 1, count_orien)
  ind_geom = where(strmatch(tag_names_data, 'GEOMETRY') eq 1, count_geom)

  if count_orien gt 0 and count_geom gt 0 then begin
     geom = data.(ind_geom)
     orien = data.(ind_orien)
     pickup_poloidal, geom, orien

     new_struct = {}
     for i = 0, n_elements(tag_names_data)-1 do begin
        if tag_names_data[i] ne 'ORIENTATION' and tag_names_data[i] ne 'GEOMETRY' then begin
           new_struct = create_struct(new_struct, tag_names_data[i], (data.(i))[0])
        endif else if tag_names_data[i] eq 'ORIENTATION' then begin
           new_struct = create_struct(new_struct, 'ORIENTATION', orien[0])
        endif else if tag_names_data[i] eq 'GEOMETRY' then begin
           new_struct = create_struct(new_struct, 'GEOMETRY', geom[0])
        endif
     endfor

     new_struct = replicate(new_struct, n_elements(data))    

     for i = 0, n_elements(tag_names_data)-1 do begin
        if tag_names_data[i] ne 'ORIENTATION' and tag_names_data[i] ne 'GEOMETRY' then begin
           data_temp = data.(i)
           if ( (size(data_temp))[0] eq 2 )     $
              and ( (size(data_temp))[1] eq 1 ) $
              and ( (size(data_temp))[2] gt 1 ) $
           then begin
              data_temp = transpose(data_temp) 
           endif
           new_struct.(i) = data_temp
        endif
     endfor

     new_struct.orientation = orien
     new_struct.geometry = geom

     return, new_struct

  endif else begin
     new_struct = {}
     for i = 0, n_elements(tag_names_data)-1 do begin
        if not_structure(data.(i)) then begin
           new_struct = create_struct(new_struct, tag_names_data[i], data.(i))
           continue
        endif
        new_struct = create_struct(new_struct, tag_names_data[i], pickuploop(data.(i)))
     endfor
  endelse

  return, new_struct

end

;------------------------------------------------------
; Retrieve all coordinates of the pickup coils, in R, Z
; and x,y,z
;------------------------------------------------------
pro getallcoords, data, R=R, x=x, y=y, z=z, $
                  unitr=unitr, unitz=unitz, $
                  colours=colours

  if not_structure(data) then return

  if undefined(x) then x = []
  if undefined(y) then y = []
  if undefined(z) then z = []
  if undefined(R) then R = []
  if undefined(unitr) then unitr = []
  if undefined(unitz) then unitz = []
  if undefined(colours) then colours = []

  tag_names_data = tag_names(data)

  ind_data = where(tag_names_data eq 'DATA', count_data)
  ind_dim = where(tag_names_data eq 'DIMENSIONS', count_dim) 

  if count_data gt 0 then begin
     R = [R, data[0].(ind_data).coordinate.R]
     Z = [Z, data[0].(ind_data).coordinate.Z]     
     
     cylindrical_cartesian, R, Z, data[0].(ind_data).coordinate.phi, x=x_here, y=y_here
     x = [x, x_here]
     x = [y, y_here]
     
     if n_elements(colours) eq 0 then colours = intarr(n_elements(R)) $
     else colours = [colours, intarr(n_elements(R))]

     ind_tor = where(strtrim(data[0].(ind_data).orientation.measurement_direction, 2) eq 'TOROIDAL', count_tor)
     if count_tor gt 0 then colours[ind_tor] = 1

     tag_names_orientation = tag_names(data[0].(ind_data).orientation)
     ind_unit = where(tag_names_orientation eq 'UNIT_VECTOR', count_unit)

     if count_unit gt 0 then begin
        unitr = [unitr, data[0].(ind_data).orientation.unit_vector.r]
        unitz = [unitz, data[0].(ind_data).orientation.unit_vector.z]
     endif

     ind_pol_angle = where(tag_names_orientation eq 'POLOIDAL_ANGLE', count_pol_angle)

     if count_pol_angle gt 0 and strtrim(data[0].(ind_data).orientation.measurement_direction, 2) eq 'POLOIDAL' then begin        
        pol_angle = data[0].(ind_data).orientation.poloidal_angle
        unitr = [unitr, cos(!PI * (360.0 - pol_angle) / 180.0)]
        unitz = [unitz, sin(!PI * (360.0 - pol_angle) / 180.0)]
     endif else if count_pol_angle gt 0 then begin
        unitr = [unitr, 0.0]
        unitz = [unitz, 0.0]
     endif
  endif else begin
     for i = 0, n_elements(tag_names_data)-1 do begin
        getallcoords, data.(i), R=R, x=x, y=y, z=z, unitr=unitr, unitz=unitz, colours=colours
     endfor
  endelse
  
end

;------------------------------------------------------
; Plot the pickup coils in R, Z
;------------------------------------------------------
pro pickupplot, data
  ; Retrieve all positions in R, Z
  R = []
  z = []
  unit_r = []
  unit_z = []
  colours = []

  getallcoords, data, R=R, z=z, unitr=unit_r, unitz=unit_z, colours=colours

  ind_high = where(Z gt 2.0)

  if n_elements(R) eq 0 or n_elements(z) eq 0  then return
                                                  
  ; Set up graphics
  gcolour                                       = [[  0,   0,   0],                                                        $
                                                   [255, 255, 255],                                                        $
                                                   [200, 200, 200],                                                        $
                                                   [ 50,  50, 255],                                                        $
                                                   [  0, 200,   0],                                                        $
                                                   [255, 175,   0],                                                        $
                                                   [255,  50,  50],                                                        $
                                                   [225,   0, 225]]

;  set_plot, 'x'
  device, decomposed=0
  tvlct,              transpose(gcolour)
  !p.background                                = 1
  !p.color                                     = 0

  ; 2D plot
  window, 0, xsize=800, ysize=800
  plot, R, z, /nodata  
  plots, R, z, psym=6, symsize=1, color=colours

  if n_elements(unit_r) ne n_elements(r) then begin
     print, 'No unit vectors'
     return
  endif

  ; arrows
  for i = 0, n_elements(unit_r)-1 do begin
     start_r = r[i]
     start_z = z[i]     


     if abs(unit_r[i]) gt 0 or abs(unit_z[i]) gt 0 then begin
        end_r = start_r + unit_r[i] * 0.1
        end_z = start_z + unit_z[i] * 0.1

        arrow, start_r, start_z, end_r, end_z, /data
     endif
  endfor

end

;------------------------------------------------------
; Main function for data manipulation.
;------------------------------------------------------
function pickupmanipulation, datastruct, poloidal=poloidal, plot=plot  

  if keyword_set(poloidal) then begin
     newstruct = pickuploop(datastruct) 
  endif else newstruct = datastruct

  ; These are mostly intended for testing.
  if keyword_set(plot) then pickupplot, newstruct

  return, newstruct
end

